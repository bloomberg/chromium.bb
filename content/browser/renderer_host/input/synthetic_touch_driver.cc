// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/synthetic_touch_driver.h"

#include "content/browser/renderer_host/input/synthetic_gesture_target.h"

namespace content {

SyntheticTouchDriver::SyntheticTouchDriver() {
  std::fill(index_map_.begin(), index_map_.end(), -1);
}

SyntheticTouchDriver::SyntheticTouchDriver(SyntheticWebTouchEvent touch_event)
    : touch_event_(touch_event) {
  std::fill(index_map_.begin(), index_map_.end(), -1);
}

SyntheticTouchDriver::~SyntheticTouchDriver() {}

void SyntheticTouchDriver::DispatchEvent(SyntheticGestureTarget* target,
                                         const base::TimeTicks& timestamp) {
  touch_event_.SetTimeStamp(timestamp);
  if (touch_event_.GetType() != blink::WebInputEvent::kUndefined)
    target->DispatchInputEventToPlatform(touch_event_);
  touch_event_.ResetPoints();
  ResetIndexMap();
}

void SyntheticTouchDriver::Press(float x,
                                 float y,
                                 int index,
                                 SyntheticPointerActionParams::Button button) {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, blink::WebTouchEvent::kTouchesLengthCap);
  int touch_index = touch_event_.PressPoint(x, y);
  index_map_[index] = touch_index;
}

void SyntheticTouchDriver::Move(float x, float y, int index) {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, blink::WebTouchEvent::kTouchesLengthCap);
  touch_event_.MovePoint(index_map_[index], x, y);
}

void SyntheticTouchDriver::Release(
    int index,
    SyntheticPointerActionParams::Button button) {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, blink::WebTouchEvent::kTouchesLengthCap);
  touch_event_.ReleasePoint(index_map_[index]);
  index_map_[index] = -1;
}

bool SyntheticTouchDriver::UserInputCheck(
    const SyntheticPointerActionParams& params) const {
  if (params.index() < 0 ||
      params.index() >= blink::WebTouchEvent::kTouchesLengthCap)
    return false;

  if (params.pointer_action_type() ==
      SyntheticPointerActionParams::PointerActionType::NOT_INITIALIZED) {
    return false;
  }

  if (params.pointer_action_type() ==
          SyntheticPointerActionParams::PointerActionType::PRESS &&
      index_map_[params.index()] >= 0) {
    return false;
  }

  if (params.pointer_action_type() ==
          SyntheticPointerActionParams::PointerActionType::MOVE &&
      index_map_[params.index()] == -1) {
    return false;
  }

  if (params.pointer_action_type() ==
          SyntheticPointerActionParams::PointerActionType::RELEASE &&
      index_map_[params.index()] == -1) {
    return false;
  }

  return true;
}

void SyntheticTouchDriver::ResetIndexMap() {
  unsigned free_index = 0;
  for (unsigned int i = 0; i < blink::WebTouchEvent::kTouchesLengthCap; ++i) {
    if (free_index >= touch_event_.touches_length)
      break;
    if (touch_event_.touches[i].state !=
        blink::WebTouchPoint::kStateUndefined) {
      touch_event_.touches[free_index] = touch_event_.touches[i];
      int index = GetIndexFromMap(i);
      index_map_[index] = free_index;
      free_index++;
    }
  }
}

int SyntheticTouchDriver::GetIndexFromMap(int value) const {
  int index = std::find(index_map_.begin(), index_map_.end(), value) -
              index_map_.begin();
  DCHECK(index >= 0 && index < blink::WebTouchEvent::kTouchesLengthCap);
  return index;
}

}  // namespace content
