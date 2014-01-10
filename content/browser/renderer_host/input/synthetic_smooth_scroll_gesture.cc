// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/synthetic_smooth_scroll_gesture.h"

#include "base/logging.h"
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
    const base::TimeTicks& timestamp, SyntheticGestureTarget* target) {
  if (state_ == SETUP) {
    gesture_source_type_ = params_.gesture_source_type;
    if (gesture_source_type_ == SyntheticGestureParams::DEFAULT_INPUT)
      gesture_source_type_ = target->GetDefaultSyntheticGestureSourceType();

    if (!target->SupportsSyntheticGestureSourceType(gesture_source_type_))
      return SyntheticGesture::GESTURE_SOURCE_TYPE_NOT_SUPPORTED_BY_PLATFORM;

    state_ = STARTED;
    start_time_ = timestamp;
  }

  DCHECK_NE(gesture_source_type_, SyntheticGestureParams::DEFAULT_INPUT);
  if (gesture_source_type_ == SyntheticGestureParams::TOUCH_INPUT)
    ForwardTouchInputEvents(timestamp, target);
  else if (gesture_source_type_ == SyntheticGestureParams::MOUSE_INPUT)
    ForwardMouseInputEvents(timestamp, target);
  else
    return SyntheticGesture::GESTURE_SOURCE_TYPE_NOT_IMPLEMENTED;

  return (state_ == DONE) ? SyntheticGesture::GESTURE_FINISHED
                          : SyntheticGesture::GESTURE_RUNNING;
}

void SyntheticSmoothScrollGesture::ForwardTouchInputEvents(
    const base::TimeTicks& timestamp, SyntheticGestureTarget* target) {
  base::TimeTicks event_timestamp = timestamp;
  switch (state_) {
    case STARTED:
      // Check for an early finish.
      if (params_.distance.IsZero()) {
        state_ = DONE;
        break;
      }
      AddTouchSlopToDistance(target);
      ComputeAndSetStopScrollingTime();
      PressTouchPoint(target, event_timestamp);
      state_ = MOVING;
      break;
    case MOVING: {
      event_timestamp = ClampTimestamp(timestamp);
      gfx::Vector2dF delta = GetPositionDeltaAtTime(event_timestamp);
      MoveTouchPoint(target, delta, event_timestamp);

      if (HasScrolledEntireDistance(event_timestamp)) {
        if (params_.prevent_fling) {
          state_ = STOPPING;
        } else {
          ReleaseTouchPoint(target, event_timestamp);
          state_ = DONE;
        }
      }
    } break;
    case STOPPING:
      if (timestamp - stop_scrolling_time_ >=
          target->PointerAssumedStoppedTime()) {
        event_timestamp =
            stop_scrolling_time_ + target->PointerAssumedStoppedTime();
        // Send one last move event, but don't change the location. Without this
        // we'd still sometimes cause a fling on Android.
        ForwardTouchEvent(target, event_timestamp);
        ReleaseTouchPoint(target, event_timestamp);
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
    const base::TimeTicks& timestamp, SyntheticGestureTarget* target) {
  switch (state_) {
    case STARTED:
      // Check for an early finish.
      if (params_.distance.IsZero()) {
        state_ = DONE;
        break;
      }
      ComputeAndSetStopScrollingTime();
      state_ = MOVING;
      // Fall through to forward the first event.
    case MOVING: {
      // Even though WebMouseWheelEvents take floating point deltas,
      // internally the scroll position is stored as an integer. We therefore
      // keep track of the discrete delta which is consistent with the
      // internal scrolling state. This ensures that when the gesture has
      // finished we've scrolled exactly the specified distance.
      base::TimeTicks event_timestamp = ClampTimestamp(timestamp);
      gfx::Vector2dF total_delta = GetPositionDeltaAtTime(event_timestamp);
      gfx::Vector2d delta_discrete =
          FloorTowardZero(total_delta - total_delta_discrete_);
      ForwardMouseWheelEvent(target, delta_discrete, event_timestamp);
      total_delta_discrete_ += delta_discrete;

      if (HasScrolledEntireDistance(event_timestamp))
        state_ = DONE;
    } break;
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
    SyntheticGestureTarget* target, const base::TimeTicks& timestamp) {
  touch_event_.timeStampSeconds = ConvertTimestampToSeconds(timestamp);

  target->DispatchInputEventToPlatform(touch_event_);
}

void SyntheticSmoothScrollGesture::ForwardMouseWheelEvent(
    SyntheticGestureTarget* target,
    const gfx::Vector2dF& delta,
    const base::TimeTicks& timestamp) const {
  blink::WebMouseWheelEvent mouse_wheel_event =
      SyntheticWebMouseWheelEventBuilder::Build(delta.x(), delta.y(), 0, false);

  mouse_wheel_event.x = params_.anchor.x();
  mouse_wheel_event.y = params_.anchor.y();

  mouse_wheel_event.timeStampSeconds = ConvertTimestampToSeconds(timestamp);

  target->DispatchInputEventToPlatform(mouse_wheel_event);
}

void SyntheticSmoothScrollGesture::PressTouchPoint(
    SyntheticGestureTarget* target, const base::TimeTicks& timestamp) {
  touch_event_.PressPoint(params_.anchor.x(), params_.anchor.y());
  ForwardTouchEvent(target, timestamp);
}

void SyntheticSmoothScrollGesture::MoveTouchPoint(
    SyntheticGestureTarget* target,
    const gfx::Vector2dF& delta,
    const base::TimeTicks& timestamp) {
  gfx::PointF touch_position = params_.anchor + delta;
  touch_event_.MovePoint(0, touch_position.x(), touch_position.y());
  ForwardTouchEvent(target, timestamp);
}

void SyntheticSmoothScrollGesture::ReleaseTouchPoint(
    SyntheticGestureTarget* target, const base::TimeTicks& timestamp) {
  touch_event_.ReleasePoint(0);
  ForwardTouchEvent(target, timestamp);
}

void SyntheticSmoothScrollGesture::AddTouchSlopToDistance(
    SyntheticGestureTarget* target) {
  // Android uses euclidean distance to compute if a touch pointer has moved
  // beyond the slop, while Aura uses Manhattan distance. We're using Euclidean
  // distance and round up to the nearest integer.
  // For vertical and horizontal scrolls (the common case), both methods produce
  // the same result.
  gfx::Vector2dF touch_slop_delta =
      ProjectLengthOntoScrollDirection(target->GetTouchSlopInDips());
  params_.distance += CeilFromZero(touch_slop_delta);
}

gfx::Vector2dF SyntheticSmoothScrollGesture::GetPositionDeltaAtTime(
    const base::TimeTicks& timestamp) const {
  // Make sure the final delta is correct. Using the computation below can lead
  // to issues with floating point precision.
  if (HasScrolledEntireDistance(timestamp))
    return -params_.distance;

  float delta_length =
      params_.speed_in_pixels_s * (timestamp - start_time_).InSecondsF();
  return -ProjectLengthOntoScrollDirection(delta_length);
}

gfx::Vector2dF SyntheticSmoothScrollGesture::ProjectLengthOntoScrollDirection(
    float delta_length) const {
  const float kTotalLength = params_.distance.Length();
  return ScaleVector2d(params_.distance, delta_length / kTotalLength);
}

void SyntheticSmoothScrollGesture::ComputeAndSetStopScrollingTime() {
  int64 total_duration_in_us = static_cast<int64>(
      1e6 * (params_.distance.Length() / params_.speed_in_pixels_s));
  DCHECK_GT(total_duration_in_us, 0);
  stop_scrolling_time_ =
      start_time_ + base::TimeDelta::FromMicroseconds(total_duration_in_us);
}

base::TimeTicks SyntheticSmoothScrollGesture::ClampTimestamp(
    const base::TimeTicks& timestamp) const {
  return std::min(timestamp, stop_scrolling_time_);
}

bool SyntheticSmoothScrollGesture::HasScrolledEntireDistance(
    const base::TimeTicks& timestamp) const {
  return timestamp >= stop_scrolling_time_;
}

}  // namespace content
