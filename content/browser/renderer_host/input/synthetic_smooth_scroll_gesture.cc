// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/synthetic_smooth_scroll_gesture.h"

#include "base/logging.h"
#include "content/common/input/input_event.h"
#include "ui/events/latency_info.h"
#include "ui/gfx/point_f.h"

namespace content {
namespace {

gfx::Vector2d FloorTowardZero(const gfx::Vector2dF& vector) {
  int x = vector.x() > 0 ? floor(vector.x()) : ceil(vector.x());
  int y = vector.y() > 0 ? floor(vector.y()) : ceil(vector.y());
  return gfx::Vector2d(x, y);
}

gfx::Vector2d CeilFromZero(const gfx::Vector2dF& vector) {
  int x = vector.x() > 0 ? ceil(vector.x()) : floor(vector.x());
  int y = vector.y() > 0 ? ceil(vector.y()) : floor(vector.y());
  return gfx::Vector2d(x, y);
}

}  // namespace

SyntheticSmoothScrollGesture::SyntheticSmoothScrollGesture(
    const SyntheticSmoothScrollGestureParams& params)
    : params_(params),
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
      if (params_.distance.IsZero()) {
        state_ = DONE;
        break;
      }
      AddTouchSlopToDistance(target);
      PressTouchPoint(target);
      state_ = MOVING;
      break;
    case MOVING:
      total_delta_ += GetPositionDelta(interval);
      MoveTouchPoint(target);

      if (HasScrolledEntireDistance()) {
        if (params_.prevent_fling) {
          state_ = STOPPING;
        } else {
          ReleaseTouchPoint(target);
          state_ = DONE;
        }
      }
      break;
    case STOPPING:
      total_stopping_wait_time_ += interval;
      if (total_stopping_wait_time_ >= target->PointerAssumedStoppedTime()) {
        // Send one last move event, but don't change the location. Without this
        // we'd still sometimes cause a fling on Android.
        MoveTouchPoint(target);
        ReleaseTouchPoint(target);
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
      if (params_.distance.IsZero()) {
        state_ = DONE;
        break;
      }
      state_ = MOVING;
      // Fall through to forward the first event.
    case MOVING:
      {
        // Even though WebMouseWheelEvents take floating point deltas,
        // internally the scroll position is stored as an integer. We therefore
        // keep track of the discrete delta which is consistent with the
        // internal scrolling state. This ensures that when the gesture has
        // finished we've scrolled exactly the specified distance.
        total_delta_ += GetPositionDelta(interval);
        gfx::Vector2d delta_discrete =
            FloorTowardZero(total_delta_ - total_delta_discrete_);
        ForwardMouseWheelEvent(target, delta_discrete);
        total_delta_discrete_ += delta_discrete;
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
    SyntheticGestureTarget* target, const gfx::Vector2dF& delta) const {
  blink::WebMouseWheelEvent mouse_wheel_event =
      SyntheticWebMouseWheelEventBuilder::Build(delta.x(), delta.y(), 0, false);

  mouse_wheel_event.x = params_.anchor.x();
  mouse_wheel_event.y = params_.anchor.y();

  target->DispatchInputEventToPlatform(
      InputEvent(mouse_wheel_event, ui::LatencyInfo(), false));
}

void SyntheticSmoothScrollGesture::PressTouchPoint(
    SyntheticGestureTarget* target) {
  touch_event_.PressPoint(params_.anchor.x(), params_.anchor.y());
  ForwardTouchEvent(target);
}

void SyntheticSmoothScrollGesture::MoveTouchPoint(
    SyntheticGestureTarget* target) {
  gfx::PointF touch_position = params_.anchor + total_delta_;
  touch_event_.MovePoint(0, touch_position.x(), touch_position.y());
  ForwardTouchEvent(target);
}

void SyntheticSmoothScrollGesture::ReleaseTouchPoint(
    SyntheticGestureTarget* target) {
  touch_event_.ReleasePoint(0);
  ForwardTouchEvent(target);
}

void SyntheticSmoothScrollGesture::AddTouchSlopToDistance(
    SyntheticGestureTarget* target) {
  // Android uses euclidean distance to compute if a touch pointer has moved
  // beyond the slop, while Aura uses Manhattan distance. We're using Euclidean
  // distance and round up to the nearest integer.
  // For vertical and horizontal scrolls (the common case), both methods produce
  // the same result.
  gfx::Vector2dF touch_slop_delta = ProjectLengthOntoScrollDirection(
      target->GetTouchSlopInDips());
  params_.distance += CeilFromZero(touch_slop_delta);
}

gfx::Vector2dF SyntheticSmoothScrollGesture::GetPositionDelta(
    const base::TimeDelta& interval) const {
  float delta_length = params_.speed_in_pixels_s * interval.InSecondsF();

  // Make sure we're not scrolling too far.
  gfx::Vector2dF remaining_delta = ComputeRemainingDelta();
  if (delta_length > remaining_delta.Length())
    // In order to scroll in a certain direction we need to move the
    // touch pointer/mouse wheel in the opposite direction.
    return -remaining_delta;
  else
    return -ProjectLengthOntoScrollDirection(delta_length);
}

gfx::Vector2dF SyntheticSmoothScrollGesture::ProjectLengthOntoScrollDirection(
    float delta_length) const {
  const float kTotalLength = params_.distance.Length();
  return ScaleVector2d(params_.distance, delta_length / kTotalLength);
}

gfx::Vector2dF SyntheticSmoothScrollGesture::ComputeRemainingDelta() const {
  return params_.distance + total_delta_;
}

bool SyntheticSmoothScrollGesture::HasScrolledEntireDistance() const {
  return ComputeRemainingDelta().IsZero();
}

}  // namespace content
