#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "QuartzRegularDB12pt7b.h"   // <-- Ensure this font in your sketch folder

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32

// SOLDERED PIN CONFIGURATION (ADC1 Only - No Wi-Fi Conflicts)
#define VOLT_PIN       35  // Battery Voltage tracker
#define POT_PIN        32  // Instrument cluster dimmer circuit
#define OIL_TEMP_PIN   34  // NTC temperature sensor signal

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

const float SERIES_RESISTOR = 10000.0; // Your 10k resistor to GND

// FINE-TUNING CALIBRATION
// Adjust this value if your readings are slightly off at operating temperature.
// Currently set to +4.0 to correct the 52C -> 56C discrepancy.
const float TEMP_CALIBRATION_OFFSET = 4.0; 

struct NTCPoint {
  float temp;
  float resistance;
};

// RESTORED ORIGINAL FACTORY NTC TABLE
const int TABLE_SIZE = 11;
NTCPoint table[TABLE_SIZE] = {
  {22, 56200}, {45, 21600}, {47, 20200}, {53, 16000}, {62, 11400},
  {71, 8500},  {75, 7200},  {83, 5500},  {87, 4700},  {90, 4500}, {92, 4200}
};

// Flag to store sensor connection state
bool sensorDisconnected = false;

float readNtcResistance() {
  long sum = 0;
  for (int i = 0; i < 20; i++) {
    sum += analogRead(OIL_TEMP_PIN);
    delay(2);
  }
  float rawAdc = sum / 20.0;
  
  // With your wiring, a disconnected sensor pulls GPIO34 down to 0V (GND) through the 10k resistor.
  if (rawAdc <= 50) { 
    sensorDisconnected = true;
    return -1.0; 
  } 
  
  sensorDisconnected = false;
  
  // CORRECTED FORMULA FOR HIGH-SIDE SENSOR / LOW-SIDE RESISTOR
  return SERIES_RESISTOR * ((4095.0 - rawAdc) / rawAdc);
}

float calculateTemperature(float currentResistance) {
  if (sensorDisconnected || currentResistance <= 0) {
    return 0; 
  }

  // Array boundary protection
  if (currentResistance >= table[0].resistance) return table[0].temp; 
  if (currentResistance <= table[TABLE_SIZE - 1].resistance) return table[TABLE_SIZE - 1].temp;

  for (int i = 0; i < TABLE_SIZE - 1; i++) {
    if (currentResistance <= table[i].resistance && currentResistance >= table[i + 1].resistance) {
      float r0 = table[i].resistance;
      float r1 = table[i + 1].resistance;
      float t0 = table[i].temp;
      float t1 = table[i + 1].temp;
      return t0 + (currentResistance - r0) * ((t1 - t0) / (r1 - r0));
    }
  }
  return 0; 
}

float readVoltage() {
  long sum = 0;
  for (int i = 0; i < 20; i++) {
    sum += analogRead(VOLT_PIN);
    delay(2);
  }
  float raw = sum / 20.0;
  float adcVoltage = (raw / 4095.0) * 3.3;
  
  // FINE-TUNED MULTI-POINT CORRECTION
  float realVoltage = (3.8501 * adcVoltage) + 3.128;
  
  return realVoltage;
}

void setOledBrightness(uint8_t value) {
  display.ssd1306_command(SSD1306_SETCONTRAST);
  display.ssd1306_command(value);
}

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  analogReadResolution(12);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    while (true); 
  }

  display.setRotation(2); 
}

void loop() {
  // ==========================================
  // SMART HEADLIGHT & INVERTED POTENTIOMETER LOGIC
  // ==========================================
  int potValue = analogRead(POT_PIN);

  if (potValue < 200) {
    setOledBrightness(255); 
  } else {
    int brightness = map(potValue, 200, 4095, 10, 200); 
    setOledBrightness(brightness);
  }

  // ==========================================
  // SENSOR DATA READINGS
  // ==========================================
  float voltage = readVoltage();
  float ntcResistance = readNtcResistance();
  float oilTemp = calculateTemperature(ntcResistance);

  // APPLY ADC HARDWARE CALIBRATION OFFSET
  if (!sensorDisconnected && oilTemp > 0) {
    oilTemp += TEMP_CALIBRATION_OFFSET;
  }

  // ==========================================
  // DISPLAY RENDERING
  // ==========================================
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // VOLTAGE DISPLAY (CUSTOM FONT)
  display.setFont(&QuartzRegularDB12pt7b);
  display.setCursor(0, 16);   
  display.print(voltage, 1);
  display.print("V");

  // OIL TEMPERATURE DISPLAY (CUSTOM FONT)
  display.setCursor(73, 16); 
  
  if (sensorDisconnected || oilTemp == 0) {
    display.print("---"); 
  } else {
    display.print(oilTemp, 0); 
  }
  
  display.fillCircle(display.getCursorX() + 2, 2, 1, SSD1306_WHITE); 
  display.setCursor(display.getCursorX() + 6, 16);
  display.print("C");

  // VERTICAL CENTER SEPARATOR LINE
  display.drawLine(62, 0, 62, 32, SSD1306_WHITE); 

  // BOTTOM LABELS (DEFAULT FIXED FONT)
  display.setFont(NULL);
  display.setTextSize(1);
  
  display.setCursor(0, 23); 
  display.print("Battery");
  
  display.setCursor(73, 23); 
  display.print("Oil Temp");

  display.display();
  delay(50); 
}

