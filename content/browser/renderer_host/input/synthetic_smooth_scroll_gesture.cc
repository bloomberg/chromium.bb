// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/synthetic_smooth_scroll_gesture.h"

#include <cmath>

#include "base/logging.h"
#include "content/common/input/input_event.h"
#include "ui/events/latency_info.h"

namespace content {

SyntheticSmoothScrollGesture::SyntheticSmoothScrollGesture(
    const SyntheticSmoothScrollGestureParams& params)
    : params_(params),
      current_y_(params_.anchor.y()),
      gesture_source_type_(SyntheticGestureParams::DEFAULT_INPUT),
      state_(SETUP) {}

SyntheticSmoothScrollGesture::~SyntheticSmoothScrollGesture() {}

SyntheticGesture::Result SyntheticSmoothScrollGesture::ForwardInputEvents(
    const base::TimeDelta& interval, SyntheticGestureTarget* target) {
  if (state_ == SETUP) {
    gesture_source_type_ = params_.gesture_source_type;
    if (gesture_source_type_ == SyntheticGestureParams::DEFAULT_INPUT)
      gesture_source_type_ = target->GetDefaultSyntheticGestureSourceType();

    if (!target->SupportsSyntheticGestureSourceType(gesture_source_type_))
      return SyntheticGesture::GESTURE_SOURCE_TYPE_NOT_SUPPORTED_BY_PLATFORM;

    state_ = STARTED;
  }

  DCHECK_NE(gesture_source_type_, SyntheticGestureParams::DEFAULT_INPUT);
  if (gesture_source_type_ == SyntheticGestureParams::TOUCH_INPUT)
    ForwardTouchInputEvents(interval, target);
  else if (gesture_source_type_ == SyntheticGestureParams::MOUSE_INPUT)
    ForwardMouseInputEvents(interval, target);
  else
    return SyntheticGesture::GESTURE_SOURCE_TYPE_NOT_IMPLEMENTED;

  return (state_ == DONE) ? SyntheticGesture::GESTURE_FINISHED
                          : SyntheticGesture::GESTURE_RUNNING;
}

void SyntheticSmoothScrollGesture::ForwardTouchInputEvents(
    const base::TimeDelta& interval, SyntheticGestureTarget* target) {
  switch (state_) {
    case STARTED:
      // Check for an early finish.
      if (params_.distance == 0) {
        state_ = DONE;
        break;
      }
      if (params_.distance > 0)
        params_.distance += target->GetTouchSlopInDips();
      else
        params_.distance -= target->GetTouchSlopInDips();
      touch_event_.PressPoint(params_.anchor.x(), current_y_);
      ForwardTouchEvent(target);
      state_ = MOVING;
      break;
    case MOVING:
      current_y_ += GetPositionDelta(interval);
      touch_event_.MovePoint(0, params_.anchor.x(), current_y_);
      ForwardTouchEvent(target);

      if (HasScrolledEntireDistance())
        state_ = STOPPING;
      break;
    case STOPPING:
      total_stopping_wait_time_ += interval;
      if (total_stopping_wait_time_ >= target->PointerAssumedStoppedTime()) {
        // Send one last move event, but don't change the location. Without this
        // we'd still sometimes cause a fling on Android.
        ForwardTouchEvent(target);
        touch_event_.ReleasePoint(0);
        ForwardTouchEvent(target);
        state_ = DONE;
      }
      break;
    case SETUP:
      NOTREACHED()
          << "State STARTED invalid for synthetic scroll using touch input.";
    case DONE:
      NOTREACHED()
          << "State DONE invalid for synthetic scroll using touch input.";
  }
}

void SyntheticSmoothScrollGesture::ForwardMouseInputEvents(
    const base::TimeDelta& interval, SyntheticGestureTarget* target) {
  switch (state_) {
    case STARTED:
      // Check for an early finish.
      if (HasScrolledEntireDistance()) {
        state_ = DONE;
        break;
      }
      state_ = MOVING;
      // Fall through to forward the first event.
    case MOVING:
      {
        const float delta = floor(GetPositionDelta(interval));
        current_y_ += delta;
        ForwardMouseWheelEvent(target, delta);
      }
      if (HasScrolledEntireDistance())
        state_ = DONE;
      break;
    case SETUP:
      NOTREACHED()
          << "State STARTED invalid for synthetic scroll using touch input.";
    case STOPPING:
      NOTREACHED()
          << "State STOPPING invalid for synthetic scroll using touch input.";
    case DONE:
      NOTREACHED()
          << "State DONE invalid for synthetic scroll using touch input.";
    }
}

void SyntheticSmoothScrollGesture::ForwardTouchEvent(
    SyntheticGestureTarget* target) const {
  target->DispatchInputEventToPlatform(
      InputEvent(touch_event_, ui::LatencyInfo(), false));
}

void SyntheticSmoothScrollGesture::ForwardMouseWheelEvent(
    SyntheticGestureTarget* target, float delta) const {
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

bool SyntheticSmoothScrollGesture::HasScrolledEntireDistance() const {
  return ComputeAbsoluteRemainingDistance() == 0;
}

}  // namespace content
