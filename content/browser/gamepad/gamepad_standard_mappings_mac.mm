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

void MapperXbox360Gamepad(
    const WebKit::WebGamepad& input,
    WebKit::WebGamepad* mapped) {
  *mapped = input;
  mapped->buttons[kButtonLeftTrigger] = AxisToButton(input.axes[2]);
  mapped->buttons[kButtonRightTrigger] = AxisToButton(input.axes[5]);
  mapped->buttons[kButtonBackSelect] = input.buttons[9];
  mapped->buttons[kButtonStart] = input.buttons[8];
  mapped->buttons[kButtonLeftThumbstick] = input.buttons[6];
  mapped->buttons[kButtonRightThumbstick] = input.buttons[7];
  mapped->buttons[kButtonDpadUp] = input.buttons[11];
  mapped->buttons[kButtonDpadDown] = input.buttons[12];
  mapped->buttons[kButtonDpadLeft] = input.buttons[13];
  mapped->buttons[kButtonDpadRight] = input.buttons[14];
  mapped->buttons[kButtonMeta] = input.buttons[10];
  mapped->axes[kAxisRightStickX] = input.axes[3];
  mapped->axes[kAxisRightStickY] = input.axes[4];
  mapped->buttonsLength = kNumButtons;
  mapped->axesLength = kNumAxes;
}

void MapperPlaystationSixAxis(
    const WebKit::WebGamepad& input,
    WebKit::WebGamepad* mapped) {
  *mapped = input;
  mapped->buttons[kButtonPrimary] = input.buttons[14];
  mapped->buttons[kButtonSecondary] = input.buttons[13];
  mapped->buttons[kButtonTertiary] = input.buttons[15];
  mapped->buttons[kButtonQuaternary] = input.buttons[12];
  mapped->buttons[kButtonLeftShoulder] = input.buttons[10];
  mapped->buttons[kButtonRightShoulder] = input.buttons[11];
  mapped->buttons[kButtonLeftTrigger] = input.buttons[8];
  mapped->buttons[kButtonRightTrigger] = input.buttons[9];
  mapped->buttons[kButtonBackSelect] = input.buttons[0];
  mapped->buttons[kButtonStart] = input.buttons[3];
  mapped->buttons[kButtonLeftThumbstick] = input.buttons[1];
  mapped->buttons[kButtonRightThumbstick] = input.buttons[2];
  mapped->buttons[kButtonDpadUp] = input.buttons[4];
  mapped->buttons[kButtonDpadDown] = input.buttons[6];
  mapped->buttons[kButtonDpadLeft] = input.buttons[7];
  mapped->buttons[kButtonDpadRight] = input.buttons[5];
  mapped->buttons[kButtonMeta] = input.buttons[16];
  mapped->axes[kAxisRightStickY] = input.axes[5];

  mapped->buttonsLength = kNumButtons;
  mapped->axesLength = kNumAxes;
}

void MapperDirectInputStyle(
    const WebKit::WebGamepad& input,
    WebKit::WebGamepad* mapped) {
  *mapped = input;
  mapped->buttons[kButtonPrimary] = input.buttons[1];
  mapped->buttons[kButtonSecondary] = input.buttons[2];
  mapped->buttons[kButtonTertiary] = input.buttons[0];
  mapped->axes[kAxisRightStickY] = input.axes[5];

  // Dpad is mapped as a direction on one axis, where -1 is up and it
  // increases clockwise to 1, which is up + left. It's set to a large (> 1.f)
  // number when nothing is depressed, except on start up, sometimes it's 0.0
  // for no data, rather than the large number.
  float dir = input.axes[9];
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

  mapped->buttonsLength = kNumButtons - 1; /* no meta */
  mapped->axesLength = kNumAxes;
}

struct MappingData {
  const char* const vendor_id;
  const char* const product_id;
  GamepadStandardMappingFunction function;
} AvailableMappings[] = {
  // http://www.linux-usb.org/usb.ids
  { "045e", "028e", MapperXbox360Gamepad },     // Xbox 360 Controller
  { "045e", "028f", MapperXbox360Gamepad },     // Xbox 360 Wireless Controller
  { "046d", "c216", MapperDirectInputStyle },   // Logitech F310, D mode
  { "046d", "c218", MapperDirectInputStyle },   // Logitech F510, D mode
  { "046d", "c219", MapperDirectInputStyle },   // Logitech F710, D mode
  { "054c", "0268", MapperPlaystationSixAxis }, // Playstation SIXAXIS
  { "2222", "0060", MapperDirectInputStyle },   // Macally iShockX, analog mode
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
