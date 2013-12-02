// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/synthetic_pinch_gesture.h"

#include <cmath>

#include "base/logging.h"
#include "content/common/input/input_event.h"
#include "ui/events/latency_info.h"

namespace content {
namespace {

// TODO(dominikg): Use touch slop to compute this value.
const float kMinPointerDistance = 40.0f;

}

SyntheticPinchGesture::SyntheticPinchGesture(
    const SyntheticPinchGestureParams& params)
    : params_(params), started_(false) {
  DCHECK_GE(params_.total_num_pixels_covered, 0);

  float inner_distance_to_anchor = kMinPointerDistance / 2.0f;
  float outer_distance_to_anchor =
      inner_distance_to_anchor + params_.total_num_pixels_covered / 2.0f;

  // Move pointers away from each other to zoom in
  // or towards each other to zoom out.
  if (params_.zoom_in) {
    current_y_0_ = params_.anchor.y() - inner_distance_to_anchor;
    current_y_1_ = params_.anchor.y() + inner_distance_to_anchor;
    target_y_0_ = params_.anchor.y() - outer_distance_to_anchor;
    target_y_1_ = params_.anchor.y() + outer_distance_to_anchor;
  } else {
    current_y_0_ = params_.anchor.y() - outer_distance_to_anchor;
    current_y_1_ = params_.anchor.y() + outer_distance_to_anchor;
    target_y_0_ = params_.anchor.y() - inner_distance_to_anchor;
    target_y_1_ = params_.anchor.y() + inner_distance_to_anchor;
  }
}

SyntheticPinchGesture::~SyntheticPinchGesture() {}

SyntheticGesture::Result SyntheticPinchGesture::ForwardInputEvents(
    const base::TimeDelta& interval, SyntheticGestureTarget* target) {

  SyntheticGestureParams::GestureSourceType source =
      params_.gesture_source_type;
  if (source == SyntheticGestureParams::DEFAULT_INPUT)
    source = target->GetDefaultSyntheticGestureSourceType();

  if (!target->SupportsSyntheticGestureSourceType(source))
    return SyntheticGesture::GESTURE_SOURCE_TYPE_NOT_SUPPORTED_BY_PLATFORM;

  if (source == SyntheticGestureParams::TOUCH_INPUT)
    return ForwardTouchInputEvents(interval, target);
  else
    return SyntheticGesture::GESTURE_SOURCE_TYPE_NOT_IMPLEMENTED;
}

SyntheticGesture::Result SyntheticPinchGesture::ForwardTouchInputEvents(
    const base::TimeDelta& interval, SyntheticGestureTarget* target) {
  if (HasFinished())
    return SyntheticGesture::GESTURE_FINISHED;

  if (!started_) {
    touch_event_.PressPoint(params_.anchor.x(), current_y_0_);
    touch_event_.PressPoint(params_.anchor.x(), current_y_1_);
    ForwardTouchEvent(target);
    started_ = true;
  }

  // Compute the delta for the first pointer. The other one moves exactly
  // the same but in the opposite direction.
  float delta = GetDeltaForPointer0(interval);
  current_y_0_ += delta;
  current_y_1_ -= delta;
  // The current pointer positions are stored as float but the pointer
  // coordinates of the input event are integers. Floor both positions so that
  // in case of an odd distance one of the pointers (the one whose position goes
  // down) moves one pixel further than the other. The explicit flooring is only
  // needed for negative values.
  touch_event_.MovePoint(0, params_.anchor.x(), floor(current_y_0_));
  touch_event_.MovePoint(1, params_.anchor.x(), floor(current_y_1_));
  ForwardTouchEvent(target);

  if (HasFinished()) {
    touch_event_.ReleasePoint(0);
    touch_event_.ReleasePoint(1);
    ForwardTouchEvent(target);
    return SyntheticGesture::GESTURE_FINISHED;
  }

  return SyntheticGesture::GESTURE_RUNNING;
}

void SyntheticPinchGesture::ForwardTouchEvent(SyntheticGestureTarget* target) {
  target->DispatchInputEventToPlatform(
      InputEvent(touch_event_, ui::LatencyInfo(), false));
}

float SyntheticPinchGesture::GetDeltaForPointer0(
    const base::TimeDelta& interval) const {
  float total_abs_delta =
      params_.relative_pointer_speed_in_pixels_s * interval.InSecondsF();

  // Make sure we're not moving too far in the final step.
  total_abs_delta =
      std::min(total_abs_delta, ComputeAbsoluteRemainingDistance());

  float abs_delta_pointer_0 = total_abs_delta / 2;
  return params_.zoom_in ? -abs_delta_pointer_0 : abs_delta_pointer_0;
}

float SyntheticPinchGesture::ComputeAbsoluteRemainingDistance() const {
  float distance_0 = params_.zoom_in ? (current_y_0_ - target_y_0_)
                                     : (target_y_0_ - current_y_0_);
  DCHECK_GE(distance_0, 0);

  // Both pointers move the same overall distance at the same speed.
  return 2 * distance_0;
}

bool SyntheticPinchGesture::HasFinished() const {
  return ComputeAbsoluteRemainingDistance() == 0;
}

}  // namespace content
