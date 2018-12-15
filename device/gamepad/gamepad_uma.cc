// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/gamepad_uma.h"

#include "base/metrics/histogram_macros.h"
#include "device/gamepad/gamepad_id_list.h"

namespace device {

void RecordConnectedGamepad(uint16_t vendor_id, uint16_t product_id) {
  GamepadId id = GamepadIdList::Get().GetGamepadId(vendor_id, product_id);
  if (id != GamepadId::kUnknownGamepad)
    UMA_HISTOGRAM_ENUMERATION("Gamepad.KnownGamepadConnected", id);
}

void RecordUnknownGamepad(GamepadSource source) {
  UMA_HISTOGRAM_ENUMERATION("Gamepad.UnknownGamepadConnected", source);
}

}  // namespace device
