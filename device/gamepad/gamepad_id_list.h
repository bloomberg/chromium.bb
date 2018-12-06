// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_GAMEPAD_ID_LIST_H_
#define DEVICE_GAMEPAD_GAMEPAD_ID_LIST_H_

#include <stddef.h>
#include <stdint.h>

#include <unordered_set>
#include <vector>

#include "base/lazy_instance.h"
#include "device/gamepad/gamepad_export.h"
#include "device/gamepad/gamepad_id.h"

namespace device {

enum XInputType {
  // Not an XInput device, or unknown.
  kXInputTypeNone,
  // Microsoft Xbox compatible.
  kXInputTypeXbox,
  // Microsoft Xbox 360 compatible.
  kXInputTypeXbox360,
  // Microsoft Xbox One compatible.
  kXInputTypeXboxOne,
};

class DEVICE_GAMEPAD_EXPORT GamepadIdList {
 public:
  // Returns a singleton instance of the GamepadId list.
  static GamepadIdList& Get();

  // Returns the GamepadId enumeration value for the gamepad with the specified
  // |vendor_id| and |product_id|. If it is not listed in the enumeration,
  // returns kUnknownGamepad.
  GamepadId GetGamepadId(uint16_t vendor_id, uint16_t product_id) const;

  // Return the XInput flavor (Xbox, Xbox 360, or Xbox One) for the device with
  // the specified |vendor_id| and |product_id|, or kXInputTypeNone if the
  // device is not XInput or the XInput flavor is unknown.
  XInputType GetXInputType(uint16_t vendor_id, uint16_t product_id) const;

  // Returns the internal list of gamepad info for testing purposes.
  std::vector<std::tuple<uint16_t, uint16_t, XInputType, GamepadId>>
  GetGamepadListForTesting() const;

 private:
  friend base::LazyInstanceTraitsBase<GamepadIdList>;
  GamepadIdList();

  DISALLOW_COPY_AND_ASSIGN(GamepadIdList);
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_GAMEPAD_ID_LIST_H_
