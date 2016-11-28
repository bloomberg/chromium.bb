// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/synthetic_mouse_driver.h"

#include "content/browser/renderer_host/input/synthetic_gesture_target.h"

namespace content {

SyntheticMouseDriver::SyntheticMouseDriver() {}

SyntheticMouseDriver::~SyntheticMouseDriver() {}

void SyntheticMouseDriver::DispatchEvent(SyntheticGestureTarget* target,
                                         const base::TimeTicks& timestamp) {
  mouse_event_.timeStampSeconds = ConvertTimestampToSeconds(timestamp);
  target->DispatchInputEventToPlatform(mouse_event_);
}

int SyntheticMouseDriver::Press(float x, float y) {
  mouse_event_ = SyntheticWebMouseEventBuilder::Build(
      blink::WebInputEvent::MouseDown, x, y, 0);
  mouse_event_.clickCount = 1;
  return 0;
}

void SyntheticMouseDriver::Move(float x, float y, int index) {
  DCHECK_EQ(index, 0);
  blink::WebMouseEvent::Button button = mouse_event_.button;
  int click_count = mouse_event_.clickCount;
  mouse_event_ = SyntheticWebMouseEventBuilder::Build(
      blink::WebInputEvent::MouseMove, x, y, 0);
  mouse_event_.button = button;
  mouse_event_.clickCount = click_count;
}

void SyntheticMouseDriver::Release(int index) {
  DCHECK_EQ(index, 0);
  mouse_event_ = SyntheticWebMouseEventBuilder::Build(
      blink::WebInputEvent::MouseUp, mouse_event_.x, mouse_event_.y, 0);
  mouse_event_.clickCount = 1;
}

bool SyntheticMouseDriver::UserInputCheck(
    const SyntheticPointerActionParams& params) const {
  if (params.gesture_source_type != SyntheticGestureParams::MOUSE_INPUT)
    return false;

  if (params.pointer_action_type() ==
      SyntheticPointerActionParams::PointerActionType::NOT_INITIALIZED) {
    return false;
  }

  if (params.pointer_action_type() ==
          SyntheticPointerActionParams::PointerActionType::PRESS &&
      mouse_event_.clickCount > 0) {
    return false;
  }

  if (params.pointer_action_type() ==
          SyntheticPointerActionParams::PointerActionType::RELEASE &&
      mouse_event_.clickCount <= 0) {
    return false;
  }

  return true;
}

}  // namespace content
