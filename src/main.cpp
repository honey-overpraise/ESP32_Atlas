#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_NeoMatrix.h>
#include <Adafruit_NeoPixel.h>

#define I2C_SCL     5
#define I2C_SDA     4
#define PIN_KEY     10
#define PIN_LED     0

#define BRIGHTNESS  20
#define mw          18
#define mh          12
#define FT6336_ADDR 0x38

#define SWAP_XY     true
#define FLIP_X      true
#define FLIP_Y      false
#define TOUCH_MAX_X 300
#define TOUCH_MAX_Y 460

Adafruit_NeoMatrix *matrix = new Adafruit_NeoMatrix(mw, mh,
                                                    PIN_LED,
                                                    NEO_MATRIX_TOP + NEO_MATRIX_RIGHT + NEO_MATRIX_COLUMNS + NEO_MATRIX_PROGRESSIVE,
                                                    NEO_GRB + NEO_KHZ800);

typedef struct {
  uint16_t x;
  uint16_t y;
  bool pressed;
} TouchPoint;

TouchPoint touch[2];
int lastLedX[2] = {-1, -1};
int lastLedY[2] = {-1, -1};
// ========== 建议6：每根手指的单笔画笔颜色 ==========
uint16_t penColor[2];
bool needRefresh = false;

unsigned long lastKeyTime = 0;
#define KEY_DEBOUNCE_MS 200

void setup() {
  Serial.begin(115200);
  randomSeed(esp_random());

  pinMode(PIN_KEY, INPUT_PULLUP);

  matrix->begin();
  matrix->setTextWrap(false);
  matrix->setBrightness(BRIGHTNESS);
  matrix->clear();
  matrix->show();

  Wire.begin(I2C_SDA, I2C_SCL);

  Wire.beginTransmission(FT6336_ADDR);
  Wire.write(0x00);
  Wire.write(0x00);
  Wire.endTransmission();
  delay(10);
}

void loop() {
  needRefresh = false;

  if (digitalRead(PIN_KEY) == LOW) {
    if (millis() - lastKeyTime > KEY_DEBOUNCE_MS) {
      matrix->clear();
      needRefresh = true;
      lastLedX[0] = lastLedX[1] = -1;
      lastLedY[0] = lastLedY[1] = -1;
      lastKeyTime = millis();
    }
  }

  Wire.beginTransmission(FT6336_ADDR);
  Wire.write(0x02);
  if (Wire.endTransmission(false) == 0) {
    Wire.requestFrom((uint8_t)FT6336_ADDR, (uint8_t)11);
    if (Wire.available() >= 11) {
      uint8_t buf[11];
      for (uint8_t i = 0; i < 11; i++) {
        buf[i] = Wire.read();
      }

      uint8_t pointNum = buf[0] & 0x0F;
      touch[0].pressed = (pointNum >= 1);
      touch[1].pressed = (pointNum >= 2);

      if (touch[0].pressed) {
        touch[0].x = ((buf[1] & 0x0F) << 8) | buf[2];
        touch[0].y = ((buf[3] & 0x0F) << 8) | buf[4];
      }

      if (touch[1].pressed) {
        touch[1].x = ((buf[7] & 0x0F) << 8) | buf[8];
        touch[1].y = ((buf[9] & 0x0F) << 8) | buf[10];
      }

      for (uint8_t i = 0; i < 2; i++) {
        if (touch[i].pressed) {
          int ledX, ledY;

          if (SWAP_XY) {
            ledX = map(touch[i].y, 0, TOUCH_MAX_Y, 0, mw - 1);
            ledY = map(touch[i].x, 0, TOUCH_MAX_X, 0, mh - 1);
          } else {
            ledX = map(touch[i].x, 0, TOUCH_MAX_X, 0, mw - 1);
            ledY = map(touch[i].y, 0, TOUCH_MAX_Y, 0, mh - 1);
          }

          if (FLIP_X) ledX = mw - 1 - ledX;
          if (FLIP_Y) ledY = mh - 1 - ledY;

          ledX = constrain(ledX, 0, mw - 1);
          ledY = constrain(ledY, 0, mh - 1);

          if (ledX != lastLedX[i] || ledY != lastLedY[i]) {
            // ========== 建议6：单笔固定随机色 ==========
            // 只有新下笔时生成一次随机色，整次滑动复用颜色
            if (lastLedX[i] == -1) {
              penColor[i] = matrix->Color(random(0, 256),
                                           random(0, 256),
                                           random(0, 256));
            }
            matrix->drawPixel(ledX, ledY, penColor[i]);

            lastLedX[i] = ledX;
            lastLedY[i] = ledY;
            needRefresh = true;
          }
        } else {
          lastLedX[i] = -1;
          lastLedY[i] = -1;
        }
      }
    }
  }

  if (needRefresh) {
    matrix->show();
  }
}
