// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/synthetic_touch_driver.h"

#include "content/browser/renderer_host/input/synthetic_gesture_target.h"

using blink::WebTouchEvent;

namespace content {

SyntheticTouchDriver::SyntheticTouchDriver() {}

SyntheticTouchDriver::SyntheticTouchDriver(SyntheticWebTouchEvent touch_event)
    : touch_event_(touch_event) {}

SyntheticTouchDriver::~SyntheticTouchDriver() {}

void SyntheticTouchDriver::DispatchEvent(SyntheticGestureTarget* target,
                                         const base::TimeTicks& timestamp) {
  touch_event_.timeStampSeconds = ConvertTimestampToSeconds(timestamp);
  target->DispatchInputEventToPlatform(touch_event_);
}

int SyntheticTouchDriver::Press(float x, float y) {
  int index = touch_event_.PressPoint(x, y);
  return index;
}

void SyntheticTouchDriver::Move(float x, float y, int index) {
  touch_event_.MovePoint(index, x, y);
}

void SyntheticTouchDriver::Release(int index) {
  touch_event_.ReleasePoint(index);
}

bool SyntheticTouchDriver::UserInputCheck(
    const SyntheticPointerActionParams& params) const {
  DCHECK_GE(params.index(), -1);
  DCHECK_LT(params.index(), WebTouchEvent::kTouchesLengthCap);
  if (params.gesture_source_type != SyntheticGestureParams::TOUCH_INPUT)
    return false;

  if (params.pointer_action_type() ==
      SyntheticPointerActionParams::PointerActionType::NOT_INITIALIZED) {
    return false;
  }

  if (params.pointer_action_type() ==
          SyntheticPointerActionParams::PointerActionType::PRESS &&
      params.index() >= 0) {
    return false;
  }

  if (params.pointer_action_type() ==
          SyntheticPointerActionParams::PointerActionType::MOVE &&
      params.index() == -1) {
    return false;
  }

  if (params.pointer_action_type() ==
          SyntheticPointerActionParams::PointerActionType::RELEASE &&
      params.index() == -1) {
    return false;
  }

  return true;
}

}  // namespace content
