// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/synthetic_smooth_scroll_gesture_new.h"

#include <cmath>

#include "content/common/input/input_event.h"
#include "ui/events/latency_info.h"

namespace content {

SyntheticSmoothScrollGestureNew::SyntheticSmoothScrollGestureNew(
    const SyntheticSmoothScrollGestureParams& params)
    : params_(params),
      current_y_(params_.anchor.y()) {}

SyntheticSmoothScrollGestureNew::~SyntheticSmoothScrollGestureNew() {}

SyntheticGestureNew::Result SyntheticSmoothScrollGestureNew::ForwardInputEvents(
    const base::TimeDelta& interval, SyntheticGestureTarget* target) {

  SyntheticGestureParams::GestureSourceType source =
      params_.gesture_source_type;
  if (source == SyntheticGestureParams::DEFAULT_INPUT)
    source = target->GetDefaultSyntheticGestureSourceType();

  if (!target->SupportsSyntheticGestureSourceType(source))
    return SyntheticGestureNew::GESTURE_SOURCE_TYPE_NOT_SUPPORTED_BY_PLATFORM;

  if (source == SyntheticGestureParams::TOUCH_INPUT)
    return ForwardTouchInputEvents(interval, target);
  else if (source == SyntheticGestureParams::MOUSE_INPUT)
    return ForwardMouseInputEvents(interval, target);
  else
    return SyntheticGestureNew::GESTURE_SOURCE_TYPE_NOT_IMPLEMENTED;
}

SyntheticGestureNew::Result
SyntheticSmoothScrollGestureNew::ForwardTouchInputEvents(
    const base::TimeDelta& interval, SyntheticGestureTarget* target) {
  if (HasFinished())
    return SyntheticGestureNew::GESTURE_FINISHED;

  if (current_y_ == params_.anchor.y()) {
    touch_event_.PressPoint(params_.anchor.x(), current_y_);
    ForwardTouchEvent(target);
  }

  current_y_ += GetPositionDelta(interval);
  touch_event_.MovePoint(0, params_.anchor.x(), current_y_);
  ForwardTouchEvent(target);

  if (HasFinished()) {
    touch_event_.ReleasePoint(0);
    ForwardTouchEvent(target);
    return SyntheticGestureNew::GESTURE_FINISHED;
  }
  else {
    return SyntheticGestureNew::GESTURE_RUNNING;
  }
}

SyntheticGestureNew::Result
SyntheticSmoothScrollGestureNew::ForwardMouseInputEvents(
    const base::TimeDelta& interval, SyntheticGestureTarget* target) {
  if (HasFinished())
    return SyntheticGestureNew::GESTURE_FINISHED;

  float delta = GetPositionDelta(interval);
  current_y_ += delta;
  ForwardMouseWheelEvent(target, delta);

  if (HasFinished())
    return SyntheticGestureNew::GESTURE_FINISHED;
  else
    return SyntheticGestureNew::GESTURE_RUNNING;
}

void SyntheticSmoothScrollGestureNew::ForwardTouchEvent(
    SyntheticGestureTarget* target) {
  target->DispatchInputEventToPlatform(
      InputEvent(touch_event_, ui::LatencyInfo(), false));
}

void SyntheticSmoothScrollGestureNew::ForwardMouseWheelEvent(
    SyntheticGestureTarget* target,
    float delta) {
  blink::WebMouseWheelEvent mouse_wheel_event =
      SyntheticWebMouseWheelEventBuilder::Build(0, delta, 0, false);

  target->DispatchInputEventToPlatform(
      InputEvent(mouse_wheel_event, ui::LatencyInfo(), false));
}

float SyntheticSmoothScrollGestureNew::GetPositionDelta(
    const base::TimeDelta& interval) {
  float delta = params_.speed_in_pixels_s * interval.InSecondsF();
  // A positive value indicates scrolling down, which means the touch pointer
  // moves up or the scroll wheel moves down. In either case, the delta is
  // negative when scrolling down and positive when scrolling up.
  return (params_.distance > 0) ? -delta : delta;
}

bool SyntheticSmoothScrollGestureNew::HasFinished() {
  return abs(current_y_ - params_.anchor.y()) >= abs(params_.distance);
}

}  // namespace content
