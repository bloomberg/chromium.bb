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

// Need at least one mapper to prevent compile errors
// Remove when other mappings are added
void MapperNull(
    const blink::WebGamepad& input,
    blink::WebGamepad* mapped) {
  *mapped = input;
}

struct MappingData {
  const char* const vendor_id;
  const char* const product_id;
  GamepadStandardMappingFunction function;
} AvailableMappings[] = {
  // http://www.linux-usb.org/usb.ids
  { "0000", "0000", MapperNull },
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
