// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gamepad/gamepad_standard_mappings.h"

#include "content/common/gamepad_hardware_buffer.h"

namespace content {

namespace {

// Maps 0..65535 to -1..1.
float NormalizeDirectInputAxis(long value) {
  return (value * 1.f / 32767.5f) - 1.f;
}

float AxisNegativeAsButton(long value) {
  return (value < 32767) ? 1.f : 0.f;
}

float AxisPositiveAsButton(long value) {
  return (value > 32767) ? 1.f : 0.f;
}

void MapperDragonRiseGeneric(
    const WebKit::WebGamepad& input,
    WebKit::WebGamepad* mapped) {
  *mapped = input;
  mapped->buttons[0] = input.buttons[1];
  mapped->buttons[1] = input.buttons[2];
  mapped->buttons[2] = input.buttons[0];
  mapped->buttons[12] = input.buttons[16];
  mapped->buttons[13] = input.buttons[17];
  mapped->buttons[14] = input.buttons[18];
  mapped->buttons[15] = input.buttons[19];
  mapped->buttonsLength = 16;
  mapped->axes[0] = NormalizeDirectInputAxis(input.axes[0]);
  mapped->axes[1] = NormalizeDirectInputAxis(input.axes[1]);
  mapped->axes[2] = NormalizeDirectInputAxis(input.axes[2]);
  mapped->axes[3] = NormalizeDirectInputAxis(input.axes[5]);
  mapped->axesLength = 4;
}

void MapperLogitechDualAction(
    const WebKit::WebGamepad& input,
    WebKit::WebGamepad* mapped) {
  *mapped = input;
  mapped->buttons[0] = input.buttons[1];
  mapped->buttons[1] = input.buttons[2];
  mapped->buttons[2] = input.buttons[0];
  mapped->buttons[12] = input.buttons[16];
  mapped->buttons[13] = input.buttons[17];
  mapped->buttons[14] = input.buttons[18];
  mapped->buttons[15] = input.buttons[19];
  mapped->buttonsLength = 16;
  mapped->axes[0] = NormalizeDirectInputAxis(input.axes[0]);
  mapped->axes[1] = NormalizeDirectInputAxis(input.axes[1]);
  mapped->axes[2] = NormalizeDirectInputAxis(input.axes[2]);
  mapped->axes[3] = NormalizeDirectInputAxis(input.axes[5]);
  mapped->axesLength = 4;
}

void MapperLogitechPrecision(
    const WebKit::WebGamepad& input,
    WebKit::WebGamepad* mapped) {
  *mapped = input;
  mapped->buttons[0] = input.buttons[1];
  mapped->buttons[1] = input.buttons[2];
  mapped->buttons[2] = input.buttons[0];
  mapped->buttons[kButtonLeftThumbstick] = 0;  // Not present
  mapped->buttons[kButtonRightThumbstick] = 0;  // Not present
  mapped->buttons[12] = AxisNegativeAsButton(input.axes[1]);
  mapped->buttons[13] = AxisPositiveAsButton(input.axes[1]);
  mapped->buttons[14] = AxisNegativeAsButton(input.axes[0]);
  mapped->buttons[15] = AxisPositiveAsButton(input.axes[0]);
  mapped->buttonsLength = 16;
  mapped->axesLength = 0;
}

struct MappingData {
  const char* const vendor_id;
  const char* const product_id;
  GamepadStandardMappingFunction function;
} AvailableMappings[] = {
  // http://www.linux-usb.org/usb.ids
  { "0079", "0006", MapperDragonRiseGeneric },  // DragonRise Generic USB
  { "046d", "c216", MapperLogitechDualAction },  // Logitech DualAction
  { "046d", "c21a", MapperLogitechPrecision },  // Logitech Precision
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
