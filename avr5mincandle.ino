/* 
  avr5mincandle.ino
  ATtiny202向け　キャンドルLED　自動消灯機能つき　ボタン電池駆動想定
  オーム電機のLED電池式ローソクSサイズ 8CM LED-01Sを改造することを想定

  Copyright (c) 2024 Kaz  (https://akibabara.com/blog/)
  Released under the MIT license.
  see https://opensource.org/licenses/MIT

  参考にしたサイト
    LEDキャンドルについての研究報告—LED2つで、ちょっぴりリッチなゆらぎ表現を—　by あつゆきさん
    https://www.creativity-ape.com/entry/2020/12/16/232928

  自分用メモ
    UPDIによる書込み方法
    https://burariweb.info/electronic-work/arduino-updi-writing-method.html
  Arduino IDEで設定する項目
    ボード: ATtiny...202
    Chip: ATtiny202
    Clock: 1MHz internal
    書き込み装置: jtag2updi
*/

#include <avr/sleep.h>

const int GPIO_LED1 = PIN_PA1;  // LED1のポート
const int GPIO_LED2 = PIN_PA3;  // LED2のポート
const int GPIO_WAKEUP_BTN = PIN_PA7;  // スリープ解除のポート
const uint32_t candleTime = 5 * 60 * 1000L;  // 自動消灯するまでの時間 5分

unsigned long tm = 0;
uint32_t value = 100;
const uint32_t maxValue = 255000;   // analogWrite()に与える最大値255 * 1000
const uint32_t dimmingRange = 150000; // 実際の範囲 150 * 1000
const uint32_t threshold = 65;

// なんちゃってランダム
const uint16_t vrands1[] = { 83,149,160,143,119,114,153,96,95,111 };
const uint16_t vrands2[] = { 875,879,856,882,892,874,933,897,926,839 };
const uint8_t vrnum = sizeof(vrands1) / sizeof(vrands1[0]);

// 割り込みフラグをクリア
ISR(PORTA_PORT_vect) {
  PORTA.INTFLAGS = PORT_INT7_bm; // PA7の割り込みフラグをクリアする
}

// スリープする
void sleep() {
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);  // POWER DOWNスリープモードを設定する
  noInterrupts(); // 全ての割り込みを無効にする
  sleep_enable(); // スリープモードを有効にする準備をする
  interrupts(); // 全ての割り込みを有効にする
  PORTA.PIN7CTRL = PORT_PULLUPEN_bm | PORT_ISC_LEVEL_gc;  // PA7ピンをプルアップし、低レベルで割り込みをトリガーするように設定
  sleep_cpu();  // スリープモードに入る
  
  // スリープモードからの復帰後にここからプログラムが続行する
  sleep_disable();  // スリープモードを無効にする
  PORTA.PIN7CTRL = PORT_PULLUPEN_bm;  // PA7ピンをプルアップし、ピン変更割り込みをオフにする
}

// ガンマ補正
uint8_t gammaCorrection(uint8_t num) {
  uint8_t gtable[10] = { 100,19, 150,58, 200,129, 225,180, 255,255 };
  uint8_t gtnum = sizeof(gtable) / sizeof(gtable[0]);
  uint8_t ret = 0;
  for (uint8_t i=0; i<gtnum; i+=2) {
    if (num <= gtable[i]) {
      uint8_t v0 = (i >= 2) ? gtable[i-2] : 0;
      uint8_t r0 = (i >= 2) ? gtable[i-1] : 0;
      ret = r0 + ((num - v0)*1000L / (gtable[i] - v0)) * (gtable[i+1] - r0) / 1000L;
      break; 
    }
  }
  return ret;
}

// 初期化
void setup() {
  // 使用するポートの設定
  pinMode(GPIO_LED1, OUTPUT);
  pinMode(GPIO_LED2, OUTPUT);
  pinMode(GPIO_WAKEUP_BTN, INPUT_PULLUP);

  // 使用しないポートの設定
  pinMode(PIN_PA0, INPUT_PULLUP);
  pinMode(PIN_PA2, INPUT_PULLUP);
  pinMode(PIN_PA6, INPUT_PULLUP);

  // タイマーの設定
  tm = millis() + candleTime;
}

// メイン
void loop() {
  static uint8_t index = 0;
  static uint8_t cnt = 0;

  // 一定時間経過したらスリープモードに入る
  if (tm < millis()) {
    //LEDをフェードアウト
    for (uint8_t i=192; i>0; i-=2) {
      analogWrite(GPIO_LED1, gammaCorrection(i));
      analogWrite(GPIO_LED2, gammaCorrection(i));
      delay(10);
    }
    digitalWrite(GPIO_LED1, LOW);
    digitalWrite(GPIO_LED2, LOW);
    sleep();  // スリープ
    // スリープモードからの復帰後にここからプログラムが続行するらしいので、タイマーを再設定する
    tm = millis() + candleTime;
    value = 100;
    cnt = 0;
  }

  // 1/fゆらぎの計算（floatだと容量オーバーになるのでlongで計算している）
  if (value < 500) {
    value = value + 2000L * value * value / 1000000L;
  } else if (value >= 500) {
    value = value - 2000L * ((1000L - value) * (1000L - value) / 1000L) / 1000L;
  }
  cnt++;
  if (value <= (0 + threshold)) {
    value = vrands1[index++ % vrnum];
  } else if ((1000 - threshold) <= value) {
    value = vrands2[index++ % vrnum];
  } else if (cnt > 30) {  // 規則的な繰り返しになってしまうことがあるので一定回数以上のループで新しい値を与える
    value = vrands1[index++ % vrnum];
    cnt = 0;
  }

  // ガンマ補正をした値でLEDを点灯する
  uint32_t ledValue1 = maxValue - dimmingRange + (value * dimmingRange) / 1000;
  uint32_t ledValue2 = maxValue - (value * dimmingRange) / 1000;
  analogWrite(GPIO_LED1, gammaCorrection(ledValue1 / 1000));
  analogWrite(GPIO_LED2, gammaCorrection(ledValue2 / 1000));

  delay(100);
}
