#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

#define COIN_PIN        32
#define RELAY_PIN       25
#define BUTTON_OK       26
#define BUTTON_UP       27
#define BUTTON_DOWN     14

#define EEPROM_SIZE         8
#define ADDR_DURASI_500     0
#define ADDR_DURASI_1000    4

LiquidCrystal_I2C lcd(0x37, 16, 2);

volatile int coinPulseCount = 0;
volatile unsigned long lastCoinPulseTime = 0;

unsigned long totalWaktu_detik = 0;
unsigned long waktuMulai = 0;
bool isRunning = false;

int durasi500 = 6 * 60;
int durasi1000 = 14 * 60;
const long MAX_WAKTU = 86400L;

String lastLine1 = "";
String lastLine2 = "";

enum Mode { STANDBY, RUNNING, MENU, SET_500, SET_1000 };
Mode mode = STANDBY;
int selectedMenu = 0;

void IRAM_ATTR detectCoin() {
  unsigned long now = millis();
  if (now - lastCoinPulseTime > 50) {
    coinPulseCount++;
    lastCoinPulseTime = now;
  }
}

void tampilLCD(const String &line1, const String &line2) {
  if (line1 != lastLine1 || line2 != lastLine2) {
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print(line1);
    lcd.setCursor(0, 1); lcd.print(line2);
    lastLine1 = line1;
    lastLine2 = line2;
  }
}

void tambahWaktu(int pulsa) {
  int tambahDetik = 0;

  if (pulsa == 1) {
    tambahDetik = durasi500;
  } else if (pulsa >= 2 && pulsa <= 5) {
    tambahDetik = durasi1000;
  } else {
    return;
  }

  totalWaktu_detik += tambahDetik;
  if (totalWaktu_detik > MAX_WAKTU) totalWaktu_detik = MAX_WAKTU;

  waktuMulai = millis();
  isRunning = true;
  mode = RUNNING;
  digitalWrite(RELAY_PIN, LOW); // relay ON

  tampilLCD("Koin Diterima!", "+" + String(tambahDetik / 60) + " Menit");
  delay(800);
  lastLine1 = "";  // paksa LCD update lagi setelah pesan ini
  lastLine2 = "";
}

void tampilMenu() {
  String line1, line2;
  if (selectedMenu == 0) { line1 = "->SET 500"; line2 = "  SET 1000"; }
  else if (selectedMenu == 1) { line1 = "  SET 500"; line2 = "->SET 1000"; }
  else { line1 = "  SET 1000"; line2 = "->EXIT"; }
  tampilLCD(line1, line2);
}

void handleMenu() {
  if (digitalRead(BUTTON_UP) == LOW) {
    delay(150);
    selectedMenu = (selectedMenu - 1 + 3) % 3;
    tampilMenu();
    while (digitalRead(BUTTON_UP) == LOW);
  }

  if (digitalRead(BUTTON_DOWN) == LOW) {
    delay(150);
    selectedMenu = (selectedMenu + 1) % 3;
    tampilMenu();
    while (digitalRead(BUTTON_DOWN) == LOW);
  }

  if (digitalRead(BUTTON_OK) == LOW) {
    delay(150);
    while (digitalRead(BUTTON_OK) == LOW);
    if (selectedMenu == 0) mode = SET_500;
    else if (selectedMenu == 1) mode = SET_1000;
    else mode = STANDBY;
    lcd.clear();
  }
}

void settingDurasi(int &durasi, const String &label, int addr) {
  char buf[17];
  sprintf(buf, "<- %2d m -> (OK)", durasi / 60);
  tampilLCD("Set " + label + " (Menit)", buf);

  if (digitalRead(BUTTON_UP) == LOW) {
    delay(150);
    if (durasi < 3600) durasi += 60;
    while (digitalRead(BUTTON_UP) == LOW);
  }

  if (digitalRead(BUTTON_DOWN) == LOW) {
    delay(150);
    if (durasi > 60) durasi -= 60;
    while (digitalRead(BUTTON_DOWN) == LOW);
  }

  if (digitalRead(BUTTON_OK) == LOW) {
    delay(150);
    EEPROM.put(addr, durasi);
    EEPROM.commit();
    tampilLCD("Tersimpan!", label + "=" + String(durasi / 60) + "m");
    delay(1000);
    mode = MENU;
    while (digitalRead(BUTTON_OK) == LOW);
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);
  delay(100);

  pinMode(COIN_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(COIN_PIN), detectCoin, FALLING);

  pinMode(BUTTON_OK, INPUT_PULLUP);
  pinMode(BUTTON_UP, INPUT_PULLUP);
  pinMode(BUTTON_DOWN, INPUT_PULLUP);

  lcd.init();
  lcd.backlight();
  tampilLCD("ADS PlayStation", "Memuat...");

  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(ADDR_DURASI_500, durasi500);
  EEPROM.get(ADDR_DURASI_1000, durasi1000);

  if (durasi500 < 60 || durasi500 > 3600) durasi500 = 6 * 60;
  if (durasi1000 < 60 || durasi1000 > 3600) durasi1000 = 14 * 60;
  EEPROM.put(ADDR_DURASI_500, durasi500);
  EEPROM.put(ADDR_DURASI_1000, durasi1000);
  EEPROM.commit();

  tampilLCD("Loading", "ADS PlayStation");
  delay(1000);
}

void loop() {
  // Tekan lama OK => menu
  if ((mode == STANDBY || mode == RUNNING) && digitalRead(BUTTON_OK) == LOW) {
    unsigned long startPress = millis();
    while (digitalRead(BUTTON_OK) == LOW);
    if (millis() - startPress >= 2000) {
      if (isRunning) {
        tampilLCD("PERINGATAN!", "Masih BERJALAN!");
        delay(1500);
      }
      mode = MENU;
      selectedMenu = 0;
      tampilMenu();
      return;
    }
  }

  // Koin terdeteksi
  if (mode != MENU && mode != SET_500 && mode != SET_1000) {
    if (coinPulseCount > 0 && millis() - lastCoinPulseTime > 300) {
      noInterrupts();
      int pulsa = coinPulseCount;
      coinPulseCount = 0;
      interrupts();
      tambahWaktu(pulsa);
    }
  }

  switch (mode) {
    case STANDBY:
      tampilLCD("Masukan Koin", "500=" + String(durasi500 / 60) + "m 1K=" + String(durasi1000 / 60) + "m");
      break;

    case RUNNING: {
      unsigned long berjalan = (millis() - waktuMulai) / 1000;
      long sisa = totalWaktu_detik - berjalan;
      if (sisa <= 0) {
        sisa = 0;
        isRunning = false;
        totalWaktu_detik = 0;
        digitalWrite(RELAY_PIN, HIGH);
        tampilLCD("Waktu Habis", "Masukan Koin");
        delay(1500);
        mode = STANDBY;
      } else {
        int m = sisa / 60;
        int d = sisa % 60;
        char buf[17];
        sprintf(buf, "%02d:%02d", m, d);
        tampilLCD("Sisa Waktu:", buf);
      }
      break;
    }

    case MENU:
      handleMenu();
      break;

    case SET_500:
      settingDurasi(durasi500, "500", ADDR_DURASI_500);
      break;

    case SET_1000:
      settingDurasi(durasi1000, "1000", ADDR_DURASI_1000);
      break;
  }

  delay(50);
}
