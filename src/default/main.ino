#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>

#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// Вентиляторы
const int fanPinA = 18;
const int fanPinB = 19;
const int channelA = 0;
const int channelB = 1;
const int freq = 25000;
const int resolution = 8;
int minPWM = 100; // Теперь не константа, чтобы можно было изменять
const int maxPWM = 255;

WebServer server(80);

// Текущие значения влажности/температуры
float temperature = 0.0;
float humidity = 0.0;

void setup() {
  Serial.begin(115200);
  dht.begin();

  // Настройка ШИМ
  ledcSetup(channelA, freq, resolution);
  ledcAttachPin(fanPinA, channelA);
  ledcWrite(channelA, 0);

  ledcSetup(channelB, freq, resolution);
  ledcAttachPin(fanPinB, channelB);
  ledcWrite(channelB, 0);

  // Подключение WiFi
  WiFi.softAP("ESP32_FAN_CTRL", "12345678");  // Имя сети и пароль
  IPAddress IP = WiFi.softAPIP();
  Serial.println("Access Point started. IP address: " + IP.toString());

  // Обработчики
  server.on("/", handleRoot);
  server.on("/set", handleSet);
  server.on("/data", handleData);
  server.on("/setMinPWM", handleSetMinPWM); // Новый обработчик
  server.begin();
}

void loop() {
  server.handleClient();

  // Чтение DHT каждые 2 секунды
  static unsigned long lastRead = 0;
  if (millis() - lastRead > 2000) {
    lastRead = millis();
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    if (!isnan(h) && !isnan(t)) {
      humidity = h;
      temperature = t;
    }
  }
}

void handleRoot() {
  String html = R"rawliteral(
    <!DOCTYPE html><html><head>
    <meta charset="UTF-8"><title>Fan Control</title>
    <style>
      body { font-family: sans-serif; text-align: center; padding: 20px; background: #f5f5f5; }
      h1 { margin-bottom: 10px; }
      .slider-container { margin: 20px; }
      input[type=range] { width: 300px; }
      .info { margin-top: 20px; font-size: 1.2em; }
      .pwm-container { margin: 20px; }
      input[type=number] { width: 100px; padding: 5px; }
      button { padding: 5px 10px; margin-left: 10px; }
    </style>
    <script>
      function updateFan(fan) {
        const val = document.getElementById('range' + fan).value;
        fetch(`/set?fan=${fan}&value=${val}`);
        document.getElementById('label' + fan).innerText = val + '%';
      }

      function setMinPWM() {
        const val = document.getElementById('minPWM').value;
        fetch(`/setMinPWM?value=${val}`)
          .then(res => res.text())
          .then(text => alert(text));
      }

      function refreshData() {
        fetch('/data')
          .then(res => res.json())
          .then(data => {
            document.getElementById('temp').innerText = data.temp + " °C";
            document.getElementById('hum').innerText = data.hum + " %";
          });
      }

      setInterval(refreshData, 3000);
      window.onload = refreshData;
    </script>
    </head><body>
      <h1>Управление вентиляторами</h1>

      <div class="slider-container">
        <h3>Вентилятор A: <span id="labelA">0%</span></h3>
        <input type="range" min="0" max="100" value="0" id="rangeA" oninput="updateFan('A')">
      </div>

      <div class="slider-container">
        <h3>Вентилятор B: <span id="labelB">0%</span></h3>
        <input type="range" min="0" max="100" value="0" id="rangeB" oninput="updateFan('B')">
      </div>

      <div class="pwm-container">
        <h3>Минимальный ШИМ (0-255):</h3>
        <input type="number" min="0" max="255" value=")" + String(minPWM) + R"rawliteral(" id="minPWM">
        <button onclick="setMinPWM()">Установить</button>
      </div>

      <div class="info">
        Температура: <span id="temp">...</span><br>
        Влажность: <span id="hum">...</span>
      </div>
    </body></html>
  )rawliteral";

  server.send(200, "text/html", html);
}

void handleSet() {
  String fan = server.arg("fan");
  int percent = server.arg("value").toInt();
  percent = constrain(percent, 0, 100);

  float factor = pow((float)percent / 100.0, 2.0);
  int pwmValue = (percent == 0) ? 0 : minPWM + (int)(factor * (maxPWM - minPWM));

  if (fan == "A") {
    ledcWrite(channelA, pwmValue);
  } else if (fan == "B") {
    ledcWrite(channelB, pwmValue);
  }

  Serial.printf("Fan %s → %d%% → PWM %d\n", fan.c_str(), percent, pwmValue);
  server.send(200, "text/plain", "OK");
}

void handleData() {
  String json = "{\"temp\":" + String(temperature, 1) + ",\"hum\":" + String(humidity, 1) + "}";
  server.send(200, "application/json", json);
}

void handleSetMinPWM() {
  int newMinPWM = server.arg("value").toInt();
  newMinPWM = constrain(newMinPWM, 0, 255);
  minPWM = newMinPWM;
  Serial.printf("New minPWM set to %d\n", minPWM);
  server.send(200, "text/plain", "Минимальный ШИМ установлен: " + String(minPWM));
}