// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/synthetic_smooth_scroll_gesture.h"

#include "content/common/input/input_event.h"
#include "ui/events/latency_info.h"

namespace content {

SyntheticSmoothScrollGesture::SyntheticSmoothScrollGesture(
    const SyntheticSmoothScrollGestureParams& params)
    : params_(params), current_y_(params_.anchor.y()) {}

SyntheticSmoothScrollGesture::~SyntheticSmoothScrollGesture() {}

SyntheticGesture::Result SyntheticSmoothScrollGesture::ForwardInputEvents(
    const base::TimeDelta& interval, SyntheticGestureTarget* target) {

  SyntheticGestureParams::GestureSourceType source =
      params_.gesture_source_type;
  if (source == SyntheticGestureParams::DEFAULT_INPUT)
    source = target->GetDefaultSyntheticGestureSourceType();

  if (!target->SupportsSyntheticGestureSourceType(source))
    return SyntheticGesture::GESTURE_SOURCE_TYPE_NOT_SUPPORTED_BY_PLATFORM;

  if (source == SyntheticGestureParams::TOUCH_INPUT)
    return ForwardTouchInputEvents(interval, target);
  else if (source == SyntheticGestureParams::MOUSE_INPUT)
    return ForwardMouseInputEvents(interval, target);
  else
    return SyntheticGesture::GESTURE_SOURCE_TYPE_NOT_IMPLEMENTED;
}

SyntheticGesture::Result SyntheticSmoothScrollGesture::ForwardTouchInputEvents(
    const base::TimeDelta& interval, SyntheticGestureTarget* target) {
  if (HasFinished())
    return SyntheticGesture::GESTURE_FINISHED;

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
    return SyntheticGesture::GESTURE_FINISHED;
  }

  return SyntheticGesture::GESTURE_RUNNING;
}

SyntheticGesture::Result SyntheticSmoothScrollGesture::ForwardMouseInputEvents(
    const base::TimeDelta& interval, SyntheticGestureTarget* target) {
  if (HasFinished())
    return SyntheticGesture::GESTURE_FINISHED;

  // Even though WebMouseWheelEvents take floating point deltas, internally the
  // scroll position is stored as an integer. Rounding the deltas ensures that
  // the gesture state, especially |current_y_|, is consistent with the internal
  // state.
  float delta = round(GetPositionDelta(interval));
  current_y_ += delta;
  ForwardMouseWheelEvent(target, delta);

  if (HasFinished())
    return SyntheticGesture::GESTURE_FINISHED;

  return SyntheticGesture::GESTURE_RUNNING;
}

void SyntheticSmoothScrollGesture::ForwardTouchEvent(
    SyntheticGestureTarget* target) {
  target->DispatchInputEventToPlatform(
      InputEvent(touch_event_, ui::LatencyInfo(), false));
}

void SyntheticSmoothScrollGesture::ForwardMouseWheelEvent(
    SyntheticGestureTarget* target, float delta) {
  blink::WebMouseWheelEvent mouse_wheel_event =
      SyntheticWebMouseWheelEventBuilder::Build(0, delta, 0, false);

  mouse_wheel_event.x = params_.anchor.x();
  mouse_wheel_event.y = params_.anchor.y();

  target->DispatchInputEventToPlatform(
      InputEvent(mouse_wheel_event, ui::LatencyInfo(), false));
}

float SyntheticSmoothScrollGesture::GetPositionDelta(
    const base::TimeDelta& interval) const {
  float abs_delta = params_.speed_in_pixels_s * interval.InSecondsF();

  // Make sure we're not scrolling too far.
  abs_delta = std::min(abs_delta, ComputeAbsoluteRemainingDistance());

  // A positive distance indicates scrolling down, which means the touch pointer
  // moves up or the scroll wheel moves down. In either case, the delta is
  // negative when scrolling down and positive when scrolling up.
  return params_.distance > 0 ? -abs_delta : abs_delta;
}

float SyntheticSmoothScrollGesture::ComputeAbsoluteRemainingDistance() const {
  float remaining_distance =
      params_.distance - (params_.anchor.y() - current_y_);
  float abs_remaining_distance =
      params_.distance > 0 ? remaining_distance : -remaining_distance;
  DCHECK_GE(abs_remaining_distance, 0);
  return abs_remaining_distance;
}

bool SyntheticSmoothScrollGesture::HasFinished() const {
  return ComputeAbsoluteRemainingDistance() == 0;
}

}  // namespace content
