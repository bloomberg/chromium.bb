// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gamepad/gamepad_standard_mappings.h"

#include "content/common/gamepad_hardware_buffer.h"

namespace content {

namespace {

float AxisToButton(float input) {
  return (input + 1.f) / 2.f;
}

float AxisNegativeAsButton(float input) {
  return (input < -0.5f) ? 1.f : 0.f;
}

float AxisPositiveAsButton(float input) {
  return (input > 0.5f) ? 1.f : 0.f;
}

void DpadFromAxis(blink::WebGamepad* mapped, float dir) {
  // Dpad is mapped as a direction on one axis, where -1 is up and it
  // increases clockwise to 1, which is up + left. It's set to a large (> 1.f)
  // number when nothing is depressed, except on start up, sometimes it's 0.0
  // for no data, rather than the large number.
  if (dir == 0.0f) {
    mapped->buttons[kButtonDpadUp] = 0.f;
    mapped->buttons[kButtonDpadDown] = 0.f;
    mapped->buttons[kButtonDpadLeft] = 0.f;
    mapped->buttons[kButtonDpadRight] = 0.f;
  } else {
    mapped->buttons[kButtonDpadUp] = (dir >= -1.f && dir < -0.7f) ||
                                     (dir >= .95f && dir <= 1.f);
    mapped->buttons[kButtonDpadRight] = dir >= -.75f && dir < -.1f;
    mapped->buttons[kButtonDpadDown] = dir >= -.2f && dir < .45f;
    mapped->buttons[kButtonDpadLeft] = dir >= .4f && dir <= 1.f;
  }
}

void MapperLogitechDualAction(
    const blink::WebGamepad& input,
    blink::WebGamepad* mapped) {
  *mapped = input;
  mapped->buttons[kButtonPrimary] = input.buttons[1];
  mapped->buttons[kButtonSecondary] = input.buttons[2];
  mapped->buttons[kButtonTertiary] = input.buttons[0];
  mapped->axes[kAxisRightStickY] = input.axes[5];
  DpadFromAxis(mapped, input.axes[9]);

  mapped->buttonsLength = kNumButtons;
  mapped->axesLength = kNumAxes;
}

void Mapper2Axes8Keys(
    const blink::WebGamepad& input,
    blink::WebGamepad* mapped) {
  *mapped = input;
  mapped->buttons[kButtonPrimary] = input.buttons[2];
  mapped->buttons[kButtonSecondary] = input.buttons[1];
  mapped->buttons[kButtonTertiary] = input.buttons[3];
  mapped->buttons[kButtonQuaternary] = input.buttons[0];
  mapped->buttons[kButtonDpadUp] = AxisNegativeAsButton(input.axes[1]);
  mapped->buttons[kButtonDpadDown] = AxisPositiveAsButton(input.axes[1]);
  mapped->buttons[kButtonDpadLeft] = AxisNegativeAsButton(input.axes[0]);
  mapped->buttons[kButtonDpadRight] = AxisPositiveAsButton(input.axes[0]);

    // Missing buttons
  mapped->buttons[kButtonLeftTrigger] = 0;
  mapped->buttons[kButtonRightTrigger] = 0;
  mapped->buttons[kButtonLeftThumbstick] = 0;
  mapped->buttons[kButtonRightThumbstick] = 0;
  mapped->buttons[kButtonMeta] = 0;

  mapped->buttonsLength = kNumButtons - 1;
  mapped->axesLength = 0;
}

void MapperDualshock4(
    const blink::WebGamepad& input,
    blink::WebGamepad* mapped) {
  enum Dualshock4Buttons {
    kTouchpadButton = kNumButtons,
    kNumDualshock4Buttons
  };

  *mapped = input;
  mapped->buttons[kButtonPrimary] = input.buttons[1];
  mapped->buttons[kButtonSecondary] = input.buttons[2];
  mapped->buttons[kButtonTertiary] = input.buttons[0];
  mapped->buttons[kButtonQuaternary] = input.buttons[3];
  mapped->buttons[kButtonLeftShoulder] = input.buttons[4];
  mapped->buttons[kButtonRightShoulder] = input.buttons[5];
  mapped->buttons[kButtonLeftTrigger] = AxisToButton(input.axes[3]);
  mapped->buttons[kButtonRightTrigger] = AxisToButton(input.axes[4]);
  mapped->buttons[kButtonBackSelect] = input.buttons[8];
  mapped->buttons[kButtonStart] = input.buttons[9];
  mapped->buttons[kButtonLeftThumbstick] = input.buttons[10];
  mapped->buttons[kButtonRightThumbstick] = input.buttons[11];
  mapped->buttons[kButtonMeta] = input.buttons[12];
  mapped->buttons[kTouchpadButton] = input.buttons[13];
  mapped->axes[kAxisRightStickY] = input.axes[5];
  DpadFromAxis(mapped, input.axes[9]);

  mapped->buttonsLength = kNumDualshock4Buttons;
  mapped->axesLength = kNumAxes;
}

void MapperOnLiveWireless(
    const blink::WebGamepad& input,
    blink::WebGamepad* mapped) {
  *mapped = input;
  mapped->buttons[kButtonPrimary] = input.buttons[0];
  mapped->buttons[kButtonSecondary] = input.buttons[1];
  mapped->buttons[kButtonTertiary] = input.buttons[3];
  mapped->buttons[kButtonQuaternary] = input.buttons[4];
  mapped->buttons[kButtonLeftShoulder] = input.buttons[6];
  mapped->buttons[kButtonRightShoulder] = input.buttons[7];
  mapped->buttons[kButtonLeftTrigger] = AxisToButton(input.axes[2]);
  mapped->buttons[kButtonRightTrigger] = AxisToButton(input.axes[5]);
  mapped->buttons[kButtonBackSelect] = input.buttons[10];
  mapped->buttons[kButtonStart] = input.buttons[11];
  mapped->buttons[kButtonLeftThumbstick] = input.buttons[13];
  mapped->buttons[kButtonRightThumbstick] = input.buttons[14];
  mapped->buttons[kButtonMeta] = input.buttons[12];
  mapped->axes[kAxisRightStickX] = input.axes[3];
  mapped->axes[kAxisRightStickY] = input.axes[4];
  DpadFromAxis(mapped, input.axes[9]);

  mapped->buttonsLength = kNumButtons;
  mapped->axesLength = kNumAxes;
}

struct MappingData {
  const char* const vendor_id;
  const char* const product_id;
  GamepadStandardMappingFunction function;
} AvailableMappings[] = {
  // http://www.linux-usb.org/usb.ids
  { "046d", "c216", MapperLogitechDualAction },  // Logitech DualAction
  { "0079", "0011", Mapper2Axes8Keys },  // 2Axes 8Keys Game Pad
  { "054c", "05c4", MapperDualshock4 },  // Playstation Dualshock 4
  { "2378", "1008", MapperOnLiveWireless },  // OnLive Controller (Bluetooth)
  { "2378", "100a", MapperOnLiveWireless },  // OnLive Controller (Wired)
};

}  // namespace

GamepadStandardMappingFunction GetGamepadStandardMappingFunction(
    const base::StringPiece& vendor_id,
    const base::StringPiece& product_id) {
  for (size_t i = 0; i < arraysize(AvailableMappings); ++i) {
    MappingData& item = AvailableMappings[i];
    if (vendor_id == item.vendor_id && product_id == item.product_id)
      return item.function;
  }
  return NULL;
}

}  // namespace content
