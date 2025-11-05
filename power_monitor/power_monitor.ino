// ===== SMART DC POWER MONITOR v1.3.4 =====
// USB Arduino + 12V Fan
// A0 = ACS712 OUT
// A1 = Battery divider: 10k -> A1 -> 5.1k -> GND

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

const int pinI = A0;
const int pinV = A1;

const float Vref = 5.0;
const float sensitivity = 0.185; // ACS712-5A
float zeroPoint = 2.5;

const float R1 = 10000.0;
const float R2 = 5100.0;

const float FAN_VOLTAGE = 12.0;

const int samples = 200;
const float deadbandA = 0.02;
const bool INVERT_DIRECTION = false;

float energy_Wh = 0.0;
float filteredVoltage = 0.0;

unsigned long lastCalc = 0;
bool showMainScreen = true;

float current = 0.0;
float power = 0.0;


// === Zero calibration at startup ===
void calibrateZero() {
  long acc = 0;
  for (int i = 0; i < 800; i++) {
    acc += analogRead(pinI);
    delayMicroseconds(200);
  }
  zeroPoint = (acc / 800.0) * (Vref / 1023.0);
}


// === SETUP ===
void setup() {
  lcd.init();
  lcd.backlight();
  Serial.begin(9600);

  lcd.setCursor(0,0);
  lcd.print("Calibrating...");
  calibrateZero();
  delay(700);
  lcd.clear();

  lastCalc = millis();

  Serial.println("ms,Vbat,I(A),P(W),Energy(Wh)");
}


// === LOOP ===
void loop() {

  unsigned long now = millis();


// === Always read battery voltage ===
int rawV = analogRead(pinV);
float nodeV = rawV * (Vref / 1023.0);
float batteryVoltage = nodeV * (R1 + R2) / R2;

// === Smooth voltage update (low-pass filter) ===
const float V_FILTER = 0.15;  // lower = slower movement, higher = faster
filteredVoltage = (V_FILTER * batteryVoltage) + ((1.0 - V_FILTER) * filteredVoltage);



  // === Fast current sample for energy ===
  long sumFast = 0;
  for (int i = 0; i < 30; i++) {
    sumFast += analogRead(pinI);
  }
  float vSenseFast = (sumFast / 30.0) * (Vref / 1023.0);
  float currentFast = (vSenseFast - zeroPoint) / sensitivity;
  if (fabs(currentFast) < deadbandA) currentFast = 0.0;

  float powerFast = FAN_VOLTAGE * currentFast;


  // === Energy accumulation ===
  float dtHours = (now - lastCalc) / 3600000.0;
  lastCalc = now;
  if (currentFast > 0.0) energy_Wh += powerFast * dtHours;


  // === Screen switch timing ===
  static unsigned long lastScreenSwitch = 0;
  const unsigned long SCREEN_DURATION = 3000;

  if (now - lastScreenSwitch >= SCREEN_DURATION) {
    lastScreenSwitch = now;
    showMainScreen = !showMainScreen;
    lcd.clear();

    // Slow & accurate current for screen display
    long slowSum = 0;
    for (int i = 0; i < samples; i++) {
      slowSum += analogRead(pinI);
      delayMicroseconds(150);
    }
    float vSenseSlow = (slowSum / (float)samples) * (Vref / 1023.0);
    current = (vSenseSlow - zeroPoint) / sensitivity;
    if (fabs(current) < deadbandA) current = 0.0;
    power = FAN_VOLTAGE * current;
  }


  // === Draw screens ===
  if (showMainScreen) {
    lcd.setCursor(0,0);
    lcd.print("I:");
    lcd.print(current, 2);
    lcd.print("A  P:");
    lcd.print(power, 1);
    lcd.print("W ");

    lcd.setCursor(0,1);
    lcd.print("Vbat:");
    lcd.print(filteredVoltage, 2);  // ✅ smooth movement, easy to read
    lcd.print("V ");
  }
  else {
  lcd.setCursor(0,0);
  lcd.print("Energy:");

  // Live energy update with fast refresh
  lcd.setCursor(0,1);
  lcd.print(energy_Wh, 4); // ✅ 4 decimals = smooth visible progress
  lcd.print(" Wh ");
}

  delay(10);
}
