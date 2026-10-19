// Serial-controlled one-shot camera trigger (video record start)
// Hardware: D9 -> 4.7k -> BC547B base (10k base pull-down to GND),
//           collector -> camera trigger, emitter -> common GND.
// D9 HIGH turns the transistor on and pulls the camera's trigger line LOW.
//
// Fires exactly one pulse, then stays locked until re-armed with R (or a board
// reset), so a repeated T can never send a second pulse (which on many cameras
// would stop the recording).
//
// Serial commands (115200 baud, case-insensitive, end with newline or just pause):
//   T          fire the trigger pulse (only accepted while armed)
//   R          re-arm after firing so the next T fires again
//   P<ms>      set pulse width in milliseconds (1-5000) while armed, e.g. P50
//   ?          print current settings and state
//
// Replies: OK, or ERR <reason>

const uint8_t TRIGGER_PIN = 9;
const unsigned long BAUD = 115200;
const unsigned long LINE_IDLE_MS = 50;  // treat a pause as end-of-command (for "No line ending")
const uint8_t MAX_CMD_LEN = 16;

unsigned long pulseWidthMs = 50;
bool fired = false;        // latched once the pulse starts; cleared by R or a board reset
bool pulseActive = false;
unsigned long pulseStartMs = 0;

char cmdBuf[MAX_CMD_LEN + 1];
uint8_t cmdLen = 0;
unsigned long lastByteMs = 0;

void readSerial();
void processCommand();
void startPulse();
void updatePulse();

void setup() {
  digitalWrite(TRIGGER_PIN, LOW);  // camera line idle (high) before the pin becomes an output
  pinMode(TRIGGER_PIN, OUTPUT);
  Serial.begin(BAUD);
  Serial.println(F("READY"));
}

void loop() {
  readSerial();
  updatePulse();
}

void readSerial() {
  while (Serial.available() > 0) {
    char c = Serial.read();
    lastByteMs = millis();

    if (c == '\n' || c == '\r') {
      processCommand();
    } else if (cmdLen < MAX_CMD_LEN) {
      cmdBuf[cmdLen++] = c;
    } else {
      cmdLen = 0;
      Serial.println(F("ERR too long"));
    }
  }

  if (cmdLen > 0 && millis() - lastByteMs >= LINE_IDLE_MS) {
    processCommand();
  }
}

void processCommand() {
  if (cmdLen == 0) return;  // ignore the second half of "\r\n" and empty lines
  cmdBuf[cmdLen] = '\0';
  cmdLen = 0;

  char cmd = toupper(cmdBuf[0]);

  if (cmd == 'T' && cmdBuf[1] == '\0') {
    if (fired) {
      Serial.println(F("ERR already fired, send R to re-arm"));
    } else {
      startPulse();
      Serial.println(F("OK"));
    }
  } else if (cmd == 'R' && cmdBuf[1] == '\0') {
    if (pulseActive) {
      Serial.println(F("ERR pulse in progress"));
    } else {
      fired = false;
      Serial.println(F("OK"));
    }
  } else if (cmd == 'P') {
    long ms = atol(cmdBuf + 1);
    if (fired) {
      Serial.println(F("ERR already fired"));
    } else if (ms < 1 || ms > 5000) {
      Serial.println(F("ERR pulse width must be 1-5000 ms"));
    } else {
      pulseWidthMs = ms;
      Serial.println(F("OK"));
    }
  } else if (cmd == '?') {
    Serial.print(F("pulse_ms="));
    Serial.print(pulseWidthMs);
    Serial.print(F(" fired="));
    Serial.println(fired ? 1 : 0);
  } else {
    Serial.println(F("ERR unknown command"));
  }
}

void startPulse() {
  fired = true;
  digitalWrite(TRIGGER_PIN, HIGH);  // transistor on -> trigger line LOW -> camera starts recording
  pulseStartMs = millis();
  pulseActive = true;
}

void updatePulse() {
  if (pulseActive && millis() - pulseStartMs >= pulseWidthMs) {
    digitalWrite(TRIGGER_PIN, LOW);  // transistor off -> trigger line back to 5V
    pulseActive = false;
  }
}
