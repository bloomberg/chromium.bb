// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_TOUCH_ACTION_H_
#define CC_INPUT_TOUCH_ACTION_H_

#include <cstdlib>

namespace cc {

// The current touch action specifies what accelerated browser operations
// (panning and zooming) are currently permitted via touch input.
// See http://www.w3.org/TR/pointerevents/#the-touch-action-css-property.
// This is intended to be the single canonical definition of the enum, it's used
// elsewhere in both Blink and content since touch action logic spans those
// subsystems.
// TODO(crbug.com/720553): rework this enum to enum class.
const size_t kTouchActionBits = 6;

enum TouchAction {
  // No scrolling or zooming allowed.
  kTouchActionNone = 0x0,
  kTouchActionPanLeft = 0x1,
  kTouchActionPanRight = 0x2,
  kTouchActionPanX = kTouchActionPanLeft | kTouchActionPanRight,
  kTouchActionPanUp = 0x4,
  kTouchActionPanDown = 0x8,
  kTouchActionPanY = kTouchActionPanUp | kTouchActionPanDown,
  kTouchActionPan = kTouchActionPanX | kTouchActionPanY,
  kTouchActionPinchZoom = 0x10,
  kTouchActionManipulation = kTouchActionPan | kTouchActionPinchZoom,
  kTouchActionDoubleTapZoom = 0x20,
  kTouchActionAuto = kTouchActionManipulation | kTouchActionDoubleTapZoom,
  kTouchActionMax = (1 << 6) - 1
};

inline TouchAction operator|(TouchAction a, TouchAction b) {
  return static_cast<TouchAction>(int(a) | int(b));
}

inline TouchAction& operator|=(TouchAction& a, TouchAction b) {
  return a = a | b;
}

inline TouchAction operator&(TouchAction a, TouchAction b) {
  return static_cast<TouchAction>(int(a) & int(b));
}

inline TouchAction& operator&=(TouchAction& a, TouchAction b) {
  return a = a & b;
}

}  // namespace cc

#endif  // CC_INPUT_TOUCH_ACTION_H_
