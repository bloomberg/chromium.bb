// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GAMEPAD_GAMEPAD_STANDARD_MAPPINGS_H_
#define CONTENT_BROWSER_GAMEPAD_GAMEPAD_STANDARD_MAPPINGS_H_

#include "base/strings/string_piece.h"

namespace WebKit {
class WebGamepad;
}

namespace content {

typedef void (*GamepadStandardMappingFunction)(
    const WebKit::WebGamepad& original,
    WebKit::WebGamepad* mapped);

GamepadStandardMappingFunction GetGamepadStandardMappingFunction(
    const base::StringPiece& vendor_id,
    const base::StringPiece& product_id);

// This defines our canonical mapping order for gamepad-like devices. If these
// items cannot all be satisfied, it is a case-by-case judgement as to whether
// it is better to leave the device unmapped, or to partially map it. In
// general, err towards leaving it *unmapped* so that content can handle
// appropriately.

enum CanonicalButtonIndex {
  kButtonPrimary,
  kButtonSecondary,
  kButtonTertiary,
  kButtonQuaternary,
  kButtonLeftShoulder,
  kButtonRightShoulder,
  kButtonLeftTrigger,
  kButtonRightTrigger,
  kButtonBackSelect,
  kButtonStart,
  kButtonLeftThumbstick,
  kButtonRightThumbstick,
  kButtonDpadUp,
  kButtonDpadDown,
  kButtonDpadLeft,
  kButtonDpadRight,
  kButtonMeta,
  kNumButtons
};

enum CanonicalAxisIndex {
  kAxisLeftStickX,
  kAxisLeftStickY,
  kAxisRightStickX,
  kAxisRightStickY,
  kNumAxes
};

}  // namespace content

#endif  // CONTENT_BROWSER_GAMEPAD_GAMEPAD_STANDARD_MAPPINGS_H_
