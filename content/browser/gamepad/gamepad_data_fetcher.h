// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GAMEPAD_GAMEPAD_DATA_FETCHER_H_
#define CONTENT_BROWSER_GAMEPAD_GAMEPAD_DATA_FETCHER_H_

#include <stdint.h>

#include "build/build_config.h"
#include "content/browser/gamepad/gamepad_standard_mappings.h"
#include "third_party/WebKit/public/platform/WebGamepads.h"

namespace content {

// Abstract interface for imlementing platform- (and test-) specific behaviro
// for getting the gamepad data.
class GamepadDataFetcher {
 public:
  virtual ~GamepadDataFetcher() {}
  virtual void GetGamepadData(blink::WebGamepads* pads,
                              bool devices_changed_hint) = 0;
  virtual void PauseHint(bool paused) {}

#if !defined(OS_ANDROID)
  struct PadState {
    // Gamepad data, unmapped.
    blink::WebGamepad data;

    // Functions to map from device data to standard layout, if available. May
    // be null if no mapping is available.
    GamepadStandardMappingFunction mapper;

    // Sanitization masks
    // axis_mask and button_mask are bitfields that represent the reset state of
    // each input. If a button or axis has ever reported 0 in the past the
    // corresponding bit will be set to 1.

    // If we ever increase the max axis count this will need to be updated.
    static_assert(blink::WebGamepad::axesLengthCap <=
        std::numeric_limits<uint32_t>::digits,
        "axis_mask is not large enough");
    uint32_t axis_mask;

    // If we ever increase the max button count this will need to be updated.
    static_assert(blink::WebGamepad::buttonsLengthCap <=
        std::numeric_limits<uint32_t>::digits,
        "button_mask is not large enough");
    uint32_t button_mask;
  };

  void MapAndSanitizeGamepadData(PadState* pad_state, blink::WebGamepad* pad);

 protected:
  PadState pad_state_[blink::WebGamepads::itemsLengthCap];
#endif
};

}  // namespace content

#endif  // CONTENT_BROWSER_GAMEPAD_GAMEPAD_DATA_FETCHER_H_
