// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/synthetic_pinch_gesture.h"

#include <cmath>

#include "base/logging.h"
#include "ui/events/latency_info.h"

namespace content {

SyntheticPinchGesture::SyntheticPinchGesture(
    const SyntheticPinchGestureParams& params)
    : params_(params),
      start_y_0_(0.0f),
      start_y_1_(0.0f),
      gesture_source_type_(SyntheticGestureParams::DEFAULT_INPUT),
      state_(SETUP) {
  DCHECK_GE(params_.total_num_pixels_covered, 0);
}

SyntheticPinchGesture::~SyntheticPinchGesture() {}

SyntheticGesture::Result SyntheticPinchGesture::ForwardInputEvents(
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
  else
    return SyntheticGesture::GESTURE_SOURCE_TYPE_NOT_IMPLEMENTED;

  return (state_ == DONE) ? SyntheticGesture::GESTURE_FINISHED
                          : SyntheticGesture::GESTURE_RUNNING;
}

void SyntheticPinchGesture::ForwardTouchInputEvents(
    const base::TimeTicks& timestamp, SyntheticGestureTarget* target) {
  switch (state_) {
    case STARTED:
      // Check for an early finish.
      if (params_.total_num_pixels_covered == 0) {
        state_ = DONE;
        break;
      }
      SetupCoordinatesAndStopTime(target);
      PressTouchPoints(target, timestamp);
      state_ = MOVING;
      break;
    case MOVING: {
      base::TimeTicks event_timestamp = ClampTimestamp(timestamp);
      float delta = GetDeltaForPointer0AtTime(event_timestamp);
      MoveTouchPoints(target, delta, event_timestamp);
      if (HasReachedTarget(event_timestamp)) {
        ReleaseTouchPoints(target, event_timestamp);
        state_ = DONE;
      }
    } break;
    case SETUP:
      NOTREACHED() << "State SETUP invalid for synthetic pinch.";
    case DONE:
      NOTREACHED() << "State DONE invalid for synthetic pinch.";
  }
}

void SyntheticPinchGesture::PressTouchPoints(SyntheticGestureTarget* target,
                                             const base::TimeTicks& timestamp) {
  touch_event_.PressPoint(params_.anchor.x(), start_y_0_);
  touch_event_.PressPoint(params_.anchor.x(), start_y_1_);
  ForwardTouchEvent(target, timestamp);
}

void SyntheticPinchGesture::MoveTouchPoints(SyntheticGestureTarget* target,
                                            float delta,
                                            const base::TimeTicks& timestamp) {
  // The two pointers move in opposite directions.
  float current_y_0 = start_y_0_ + delta;
  float current_y_1 = start_y_1_ - delta;

  // The current pointer positions are stored as float but the pointer
  // coordinates of the input event are integers. Floor both positions so that
  // in case of an odd distance one of the pointers (the one whose position goes
  // down) moves one pixel further than the other. The explicit flooring is only
  // needed for negative values.
  touch_event_.MovePoint(0, params_.anchor.x(), floor(current_y_0));
  touch_event_.MovePoint(1, params_.anchor.x(), floor(current_y_1));
  ForwardTouchEvent(target, timestamp);
}

void SyntheticPinchGesture::ReleaseTouchPoints(
    SyntheticGestureTarget* target, const base::TimeTicks& timestamp) {
  touch_event_.ReleasePoint(0);
  touch_event_.ReleasePoint(1);
  ForwardTouchEvent(target, timestamp);
}

void SyntheticPinchGesture::ForwardTouchEvent(
    SyntheticGestureTarget* target, const base::TimeTicks& timestamp) {
  touch_event_.timeStampSeconds = ConvertTimestampToSeconds(timestamp);
  target->DispatchInputEventToPlatform(touch_event_);
}

void SyntheticPinchGesture::SetupCoordinatesAndStopTime(
    SyntheticGestureTarget* target) {
  const int kTouchSlopInDips = target->GetTouchSlopInDips();
  params_.total_num_pixels_covered += 2 * kTouchSlopInDips;
  float inner_distance_to_anchor = 2 * kTouchSlopInDips;
  float outer_distance_to_anchor =
      inner_distance_to_anchor + params_.total_num_pixels_covered / 2.0f;

  // Move pointers away from each other to zoom in
  // or towards each other to zoom out.
  if (params_.zoom_in) {
    start_y_0_ = params_.anchor.y() - inner_distance_to_anchor;
    start_y_1_ = params_.anchor.y() + inner_distance_to_anchor;
  } else {
    start_y_0_ = params_.anchor.y() - outer_distance_to_anchor;
    start_y_1_ = params_.anchor.y() + outer_distance_to_anchor;
  }

  int64 total_duration_in_us = static_cast<int64>(
      1e6 * (static_cast<double>(params_.total_num_pixels_covered) /
             params_.relative_pointer_speed_in_pixels_s));
  DCHECK_GT(total_duration_in_us, 0);
  stop_time_ =
      start_time_ + base::TimeDelta::FromMicroseconds(total_duration_in_us);
}

float SyntheticPinchGesture::GetDeltaForPointer0AtTime(
    const base::TimeTicks& timestamp) const {
  float total_abs_delta;

  // Make sure the final delta is correct. Using the computation below can lead
  // to issues with floating point precision.
  if (HasReachedTarget(timestamp))
    total_abs_delta = params_.total_num_pixels_covered;
  else
    total_abs_delta = params_.relative_pointer_speed_in_pixels_s *
                      (timestamp - start_time_).InSecondsF();

  float abs_delta_pointer_0 = total_abs_delta / 2.0f;
  return params_.zoom_in ? -abs_delta_pointer_0 : abs_delta_pointer_0;
}

base::TimeTicks SyntheticPinchGesture::ClampTimestamp(
    const base::TimeTicks& timestamp) const {
  return std::min(timestamp, stop_time_);
}

bool SyntheticPinchGesture::HasReachedTarget(const base::TimeTicks& timestamp)
    const {
  return timestamp >= stop_time_;
}

}  // namespace content
