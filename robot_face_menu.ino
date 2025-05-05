#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <math.h> // Keep for potential future use or complex highlights

// Use U8g2 for I2C OLED display (SSD1306 common)
// Wemos D1 Mini default I2C pins: SCL=D1 (GPIO5), SDA=D2 (GPIO4)
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE, /* clock=*/ SCL, /* data=*/ SDA);

// --- Display Configuration ---
const int displayHeight = 64;
const int displayWidth = 128;

// --- Animation ---
float animationSpeed = 0.15; // Slightly slower for smoother large eyes
int currentExpressionIndex = 0; // Index for the current eye expression

// --- Automatic Expression Cycling ---
unsigned long expressionStartTime = 0;
const unsigned long expressionDuration = 4000; // Show each expression for 4 seconds

// --- Eye Expressions ---
// Define expression names (optional, mainly for Serial output)
const char *expressionNames[] = {"Neutral", "Happy Squint", "Sad Look", "Angry Glare", "Wide Open", "Sleepy", "Looking Left", "Looking Right"};
const int expressionCount = sizeof(expressionNames) / sizeof(expressionNames[0]);

// --- Eye Animation Parameters (Maximized for Screen) ---
int eyeCenterY = displayHeight / 2;
int eyeDistance = 64; // Distance between centers (128 / 2)
int leftEyeCenterX = displayWidth / 4;     // Center of left eye (128 / 4 = 32)
int rightEyeCenterX = displayWidth * 3 / 4; // Center of right eye (128 * 3 / 4 = 96)

// Base eye size (can be adjusted per expression)
int baseEyeWidth = 58; // Max width considering distance (e.g., 64 - padding)
int baseEyeHeight = 58; // Max height (e.g., 64 - padding)

// Current and Target Eye parameters
float eyeWidth = baseEyeWidth; // Use float for smoother lerp
float eyeHeight = baseEyeHeight; // Use float for smoother lerp
float targetEyeWidth = baseEyeWidth;
float targetEyeHeight = baseEyeHeight;
float eyeOpenness = 1.0;     // For blinking (1.0 = fully open)
float targetEyeOpenness = 1.0;

// Pupil parameters (relative to eye size)
int pupilSize = 25;          // Base size of pupils (adjust based on visual preference)
float pupilOffsetX = 0.0;    // Horizontal offset for looking left/right
float pupilOffsetY = 0.0;    // Vertical offset for looking up/down
float targetPupilOffsetX = 0.0; // Target horizontal offset
float targetPupilOffsetY = 0.0; // Target vertical offset
bool hasHighlight = true;    // Whether to show white highlights in eyes
int numHighlights = 2;       // How many highlights per eye

// Blinking control
bool isBlinking = false;
unsigned long lastBlinkTime = 0;
unsigned long nextBlinkTime = 0;
const int blinkDuration = 180; // Slightly faster blink?

// Random eye movement
unsigned long lastLookTime = 0;
unsigned long lookInterval = 3000; // Interval for random gaze shifts

void setup() {
  Serial.begin(115200);
  Serial.println("Maximized Robot Eyes - Auto Cycle");

  u8g2.begin();

  // Initialize random seed
  randomSeed(analogRead(A0));

  // Set first blink time
  nextBlinkTime = millis() + random(1500, 3500); // Blink sooner initially
  lastLookTime = millis() + random(800, 2000); // Initial look sooner

  // Set initial expression and start timer
  setEyeExpression(currentExpressionIndex);
  expressionStartTime = millis(); // Start the timer for the first expression

  // Instantly set current values to initial targets for immediate display
  eyeWidth = targetEyeWidth;
  eyeHeight = targetEyeHeight;
  eyeOpenness = targetEyeOpenness;
  pupilOffsetX = targetPupilOffsetX;
  pupilOffsetY = targetPupilOffsetY;

  Serial.println("Setup complete. Auto-cycling expressions...");
}

void setEyeExpression(int index) {
  if (index < 0 || index >= expressionCount) return; // Safety check

  Serial.print("Setting Expression: "); Serial.println(expressionNames[index]);

  // Reset gaze targets unless specifically set by expression
  targetPupilOffsetX = 0.0;
  targetPupilOffsetY = 0.0;
  targetEyeOpenness = 1.0; // Default to fully open unless specified

  switch(index) {
    case 0: // Neutral
      targetEyeWidth = baseEyeWidth;
      targetEyeHeight = baseEyeHeight;
      hasHighlight = true;
      numHighlights = 2;
      pupilSize = 25;
      break;

    case 1: // Happy Squint
      targetEyeWidth = baseEyeWidth;
      targetEyeHeight = baseEyeHeight * 0.6; // Squint vertically
      targetEyeOpenness = 0.7; // Also use openness for squint effect
      hasHighlight = true;
      numHighlights = 2;
      pupilSize = 24;
      targetPupilOffsetY = -2; // Slight upward look?
      break;

    case 2: // Sad Look
      targetEyeWidth = baseEyeWidth * 0.9;
      targetEyeHeight = baseEyeHeight * 0.8;
      targetEyeOpenness = 0.8;
      targetPupilOffsetY = 8;  // Look noticeably down
      hasHighlight = true;
      numHighlights = 1; // Single highlight
      pupilSize = 28; // Slightly larger pupil
      break;

    case 3: // Angry Glare
      targetEyeWidth = baseEyeWidth;
      targetEyeHeight = baseEyeHeight * 0.7; // Squint
      targetEyeOpenness = 0.75;
      targetPupilOffsetY = -5; // Look slightly up (intense)
      hasHighlight = true; // Glare needs highlight
      numHighlights = 1;
      pupilSize = 20; // Smaller pupil for focus
      break;

    case 4: // Wide Open (Surprised)
      targetEyeWidth = baseEyeWidth; // Keep width
      targetEyeHeight = baseEyeHeight; // Max height
      targetEyeOpenness = 1.1; // Try to force slightly wider if possible visually
      hasHighlight = true;
      numHighlights = 2;
      pupilSize = 32; // Large pupil
      break;

    case 5: // Sleepy
      targetEyeWidth = baseEyeWidth * 0.95;
      targetEyeHeight = baseEyeHeight * 0.3; // Very squinted
      targetEyeOpenness = 0.3;
      targetPupilOffsetY = 5; // Looking down
      hasHighlight = false; // No highlight when sleepy
      pupilSize = 26;
      break;

    case 6: // Looking Left
      targetEyeWidth = baseEyeWidth;
      targetEyeHeight = baseEyeHeight;
      targetPupilOffsetX = -12; // Look left
      targetPupilOffsetY = -2;  // Slightly up
      hasHighlight = true;
      numHighlights = 2;
      pupilSize = 25;
      break;

    case 7: // Looking Right
      targetEyeWidth = baseEyeWidth;
      targetEyeHeight = baseEyeHeight;
      targetPupilOffsetX = 12; // Look right
      targetPupilOffsetY = -2; // Slightly up
      hasHighlight = true;
      numHighlights = 2;
      pupilSize = 25;
      break;
  }
}

void handleBlinking() {
  unsigned long currentTime = millis();

  // Check if it's time to blink
  if (!isBlinking && currentTime > nextBlinkTime) {
    isBlinking = true;
    lastBlinkTime = currentTime;
  }

  // Handle blink animation
  if (isBlinking) {
    unsigned long blinkAge = currentTime - lastBlinkTime;
    float blinkProgress = (float)blinkAge / blinkDuration;

    if (blinkProgress < 0.5) { // Closing phase
      eyeOpenness = lerp(targetEyeOpenness, 0.1f, blinkProgress * 2.0);
    } else if (blinkProgress < 1.0) { // Opening phase
      eyeOpenness = lerp(0.1f, targetEyeOpenness, (blinkProgress - 0.5) * 2.0);
    } else { // Blink complete
      eyeOpenness = targetEyeOpenness;
      isBlinking = false;
      nextBlinkTime = currentTime + random(1500, 4500);
    }
  } else {
    // Smoothly animate eye openness towards the target when not blinking
    // Use a slightly faster lerp for openness to make blinks snappier if needed
    eyeOpenness = lerp(eyeOpenness, targetEyeOpenness, animationSpeed * 1.2);
  }
}

// Linear interpolation helper
float lerp(float a, float b, float t) {
  t = max(0.0f, min(1.0f, t)); // Clamp t between 0 and 1
  return a + t * (b - a);
}

void handleLooking() {
  unsigned long currentTime = millis();

  // Occasionally change looking direction (only if expression doesn't dictate gaze)
  if (!isBlinking && currentTime - lastLookTime > lookInterval && targetPupilOffsetX == 0.0 && targetPupilOffsetY == 0.0) {
    lastLookTime = currentTime;
    lookInterval = random(2500, 6000);

    int lookType = random(100);
    float maxOffsetH = baseEyeWidth * 0.15;
    float maxOffsetV = baseEyeHeight * 0.15;

    if (lookType < 40) { // Look towards center
      targetPupilOffsetX = 0.0;
      targetPupilOffsetY = 0.0;
    } else if (lookType < 70) { // Look horizontal
      targetPupilOffsetX = random(0, 2) == 0 ? random(-maxOffsetH, -maxOffsetH * 0.5) : random(maxOffsetH * 0.5, maxOffsetH);
      targetPupilOffsetY = random(-maxOffsetV * 0.3, maxOffsetV * 0.3);
    } else { // Look vertical/diagonal
      targetPupilOffsetX = random(-maxOffsetH * 0.5, maxOffsetH * 0.5);
      targetPupilOffsetY = random(0, 2) == 0 ? random(-maxOffsetV, -maxOffsetV * 0.5) : random(maxOffsetV * 0.5, maxOffsetV);
    }
  }

  // Smooth pupil movement towards targets
  pupilOffsetX = lerp(pupilOffsetX, targetPupilOffsetX, animationSpeed * 0.8);
  pupilOffsetY = lerp(pupilOffsetY, targetPupilOffsetY, animationSpeed * 0.8);
}

void drawEyes() {
  u8g2.clearBuffer();

  // Handle animations
  handleBlinking(); // Updates eyeOpenness
  handleLooking();  // Updates pupilOffsetX, pupilOffsetY

  // Smooth animations for eye size (use float versions now)
  eyeWidth = lerp(eyeWidth, targetEyeWidth, animationSpeed);
  eyeHeight = lerp(eyeHeight, targetEyeHeight, animationSpeed);

  // Calculate actual eye height based on openness
  float currentOpenness = max(0.1f, min(1.1f, eyeOpenness));
  int actualEyeHeight = round(eyeHeight * currentOpenness);
  if (actualEyeHeight < 1) actualEyeHeight = 1;

  // --- Draw Left Eye ---
  u8g2.setDrawColor(1);
  u8g2.drawFilledEllipse(leftEyeCenterX, eyeCenterY, round(eyeWidth / 2.0), round(actualEyeHeight / 2.0));

  if (actualEyeHeight > 5 && hasHighlight) {
    u8g2.setDrawColor(0);
    int hlBaseX = leftEyeCenterX + round(pupilOffsetX);
    int hlBaseY = eyeCenterY + round(pupilOffsetY);
    int hl1Size = max(1, (int)round(pupilSize * 0.20));
    int hl2Size = max(1, (int)round(pupilSize * 0.12));
    int hl1X = hlBaseX + round(pupilSize * 0.15);
    int hl1Y = hlBaseY - round(pupilSize * 0.15);
    int hl2X = hlBaseX - round(pupilSize * 0.20);
    int hl2Y = hlBaseY + round(pupilSize * 0.10);
    u8g2.drawDisc(hl1X, hl1Y, hl1Size);
    if (numHighlights >= 2) {
      u8g2.drawDisc(hl2X, hl2Y, hl2Size);
    }
  }

   // --- Draw Right Eye ---
  u8g2.setDrawColor(1);
  u8g2.drawFilledEllipse(rightEyeCenterX, eyeCenterY, round(eyeWidth / 2.0), round(actualEyeHeight / 2.0));

  if (actualEyeHeight > 5 && hasHighlight) {
    u8g2.setDrawColor(0);
    int hlBaseX = rightEyeCenterX + round(pupilOffsetX);
    int hlBaseY = eyeCenterY + round(pupilOffsetY);
    int hl1Size = max(1, (int)round(pupilSize * 0.20));
    int hl2Size = max(1, (int)round(pupilSize * 0.12));
    int hl1X = hlBaseX + round(pupilSize * 0.15);
    int hl1Y = hlBaseY - round(pupilSize * 0.15);
    int hl2X = hlBaseX - round(pupilSize * 0.20);
    int hl2Y = hlBaseY + round(pupilSize * 0.10);
    u8g2.drawDisc(hl1X, hl1Y, hl1Size);
    if (numHighlights >= 2) {
      u8g2.drawDisc(hl2X, hl2Y, hl2Size);
    }
  }

  u8g2.sendBuffer();
}

void loop() {
  unsigned long currentTime = millis();

  // Check if it's time to switch to the next expression
  if (currentTime - expressionStartTime > expressionDuration) {
    expressionStartTime = currentTime; // Reset the timer
    currentExpressionIndex++;          // Move to the next expression
    if (currentExpressionIndex >= expressionCount) {
      currentExpressionIndex = 0; // Wrap around to the beginning
    }
    setEyeExpression(currentExpressionIndex); // Apply the new expression targets
  }

  drawEyes(); // Draw the current state of the eyes (animating towards targets)

  delay(15); // ~66 FPS target, adjust as needed
}