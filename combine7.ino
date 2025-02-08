#include <PulseSensorPlayground.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_BMP3XX.h"
#include <Adafruit_BNO08x.h>

// Display configuration
#define TFT_CS    25
#define TFT_RST   27
#define TFT_DC    26
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// Pulse Sensor configuration
const int PulseWire = 35;
const int LED13 = 13;
int Threshold = 550;
PulseSensorPlayground pulseSensor;

// BMP388 configuration
#define BMP388_ADDRESS 0x77
Adafruit_BMP3XX bmp;
#define SEALEVELPRESSURE_HPA (1013.25)

// BNO08x configuration
#define BNO08X_ADDRESS 0x4A
Adafruit_BNO08x bno08x;
sh2_SensorValue_t sensorValue;

// Button configuration
const int buttonPin = 0;
int lastButtonState = HIGH;
enum DisplayMode {CLOCK, HEART_RATE, TEMP_PRESSURE, ALTITUDE, MOTION};
DisplayMode currentMode = CLOCK;

// Clock variables
unsigned long previousMillis = 0;
const long interval = 1000;
int seconds = 0, minutes = 0, hours = 0;
bool initialClockDraw = true;

void setup() {
  Serial.begin(115200);
  
  // Initialize display
  tft.init(240, 240);
  tft.setRotation(0);
  tft.fillScreen(ST77XX_BLACK);
  
  // Initialize pulse sensor
  pulseSensor.analogInput(PulseWire);
  pulseSensor.blinkOnPulse(LED13);
  pulseSensor.setThreshold(Threshold);
  if(!pulseSensor.begin()) showError("Pulse Sensor Fail");

  // Initialize I2C sensors
  Wire.begin(21, 22);
  if(!bmp.begin_I2C(BMP388_ADDRESS)) showError("BMP388 Init Fail");
  if(!bno08x.begin_I2C(BNO08X_ADDRESS)) showError("BNO08x Init Fail");
  
  // Configure sensors
  configureBMP388();
  configureBNO08x();
  
  // Configure button
  pinMode(buttonPin, INPUT_PULLUP);
}

void loop() {
  handleButton();
  updateSensors();
  updateDisplay();
  delay(100);
}

void handleButton() {
  int buttonState = digitalRead(buttonPin);
  if(buttonState != lastButtonState && buttonState == LOW) {
    currentMode = static_cast<DisplayMode>((currentMode + 1) % 5);
    tft.fillScreen(ST77XX_BLACK);
    initialClockDraw = true;
  }
  lastButtonState = buttonState;
}

void updateSensors() {
  static unsigned long lastClockUpdate = 0;
  if(millis() - lastClockUpdate >= 1000) {
    lastClockUpdate = millis();
    updateClock();
  }

  switch(currentMode) {
    case HEART_RATE:
      pulseSensor.getBeatsPerMinute();
      break;
      
    case TEMP_PRESSURE:
    case ALTITUDE:
      bmp.performReading();
      break;
      
    case MOTION:
      bno08x.getSensorEvent(&sensorValue);
      break;
      
    default: break;
  }
}

void updateDisplay() {
  switch(currentMode) {
    case CLOCK:
      drawDigitalClock();
      break;
      
    case HEART_RATE:
      displayHeartRate();
      break;
      
    case TEMP_PRESSURE:
      displayTempPressure();
      break;
      
    case ALTITUDE:
      displayAltitude();
      break;
      
    case MOTION:
      displayMotion();
      break;
  }
}


void drawDigitalClock() {
  static int lastSecond = -1;
  
  if(initialClockDraw) {
    tft.fillScreen(ST77XX_BLACK);
    drawHeader("Digital Clock");
    initialClockDraw = false;
  }

  if(seconds != lastSecond) {
    lastSecond = seconds;
    
    // 
    tft.setTextSize(4);
    tft.setTextColor(ST77XX_WHITE);
    
    // centralized text
    char timeStr[9];
    sprintf(timeStr, "%02d:%02d:%02d", hours, minutes, seconds);
    int16_t x1, y1;
    uint16_t w, h;
    tft.getTextBounds(timeStr, 0, 0, &x1, &y1, &w, &h);
    int xPos = (240 - w) / 2;
    int yPos = (240 - h) / 2;
    
    // clear old text
    tft.fillRect(xPos-5, yPos-5, w+10, h+10, ST77XX_BLACK);
    
    tft.setCursor(xPos, yPos);
    tft.print(timeStr);

    // 
    tft.setTextSize(1);
    tft.setCursor(80, 220);
    tft.print("HH:MM:SS 24H");
  }
}

// 24hrs
void updateClock() {
  unsigned long currentMillis = millis();
  if(currentMillis - previousMillis >= 1000) {
    previousMillis = currentMillis;
    
    if(++seconds >= 60) {
      seconds = 0;
      if(++minutes >= 60) {
        minutes = 0;
        hours = (hours + 1) % 24; 
      }
    }
  }
}


void displayTempPressure() {
  drawHeader("Environment");
  displayValue("Temp:", String(bmp.temperature, 1) + " C", 60, ST77XX_YELLOW);
  displayValue("Press:", String(bmp.pressure / 100.0, 1) + " hPa", 120, ST77XX_CYAN);
}

void displayAltitude() {
  drawHeader("Altitude");
  displayValue("Height:", String(bmp.readAltitude(SEALEVELPRESSURE_HPA), 1) + " m", 90, ST77XX_GREEN);
}

void displayMotion() {
  drawHeader("Motion Sensor");
  if(sensorValue.sensorId == SH2_ACCELEROMETER) {
    displayValue("X:", String(sensorValue.un.accelerometer.x, 2), 100, ST77XX_RED);
    displayValue("Y:", String(sensorValue.un.accelerometer.y, 2), 140, ST77XX_GREEN);
    displayValue("Z:", String(sensorValue.un.accelerometer.z, 2), 180, ST77XX_BLUE);
  }
}

void displayHeartRate() {
  int bpm = pulseSensor.getBeatsPerMinute();
  drawHeader("Heart Rate");
  
  tft.setCursor(20, 80);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);
  tft.print("Status: ");
  
  if(pulseSensor.sawStartOfBeat()) {
    if(bpm >= 55 && bpm <= 100) {
      tft.setTextColor(ST77XX_GREEN);
      tft.print("Detect");
      displayValue("BPM:", String(bpm), 120, ST77XX_GREEN);
    } else {
      tft.setTextColor(ST77XX_YELLOW);
      tft.print("...");
    }
  } else {
    tft.setTextColor(ST77XX_YELLOW);
    tft.print("Detecting...");
  }
}

void drawHeader(const char* title) {
  tft.fillRect(0, 0, 240, 40, ST77XX_BLUE);
  tft.setCursor(10, 10);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);
  tft.print(title);
}

void displayValue(const char* label, String value, int yPos, uint16_t color) {
  tft.fillRect(0, yPos-10, 240, 40, ST77XX_BLACK);
  tft.setCursor(20, yPos-10);
  tft.setTextColor(ST77XX_WHITE);
  tft.print(label);
  
  tft.setCursor(40, yPos+10);
  tft.setTextColor(color);
  tft.print(value);
}

void showError(const char* message) {
  tft.fillScreen(ST77XX_RED);
  tft.setCursor(20, 100);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.print(message);
  while(1) delay(1000);
}

void configureBMP388() {
  bmp.setTemperatureOversampling(BMP3_OVERSAMPLING_8X);
  bmp.setPressureOversampling(BMP3_OVERSAMPLING_4X);
  bmp.setIIRFilterCoeff(BMP3_IIR_FILTER_COEFF_3);
  bmp.setOutputDataRate(BMP3_ODR_50_HZ);
}

void configureBNO08x() {
  bno08x.enableReport(SH2_ACCELEROMETER);
  bno08x.enableReport(SH2_GYROSCOPE_CALIBRATED);
  bno08x.enableReport(SH2_MAGNETIC_FIELD_CALIBRATED);
}
