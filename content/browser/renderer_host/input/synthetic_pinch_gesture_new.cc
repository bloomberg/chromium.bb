// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/synthetic_pinch_gesture_new.h"

#include "content/common/input/input_event.h"
#include "ui/events/latency_info.h"

namespace content {
namespace {

// TODO(dominikg): Use touch slop to compute this value.
const float kMinPointerDistance = 40.0f;

}

SyntheticPinchGestureNew::SyntheticPinchGestureNew(
    const SyntheticPinchGestureParams& params)
    : params_(params), started_(false) {
  DCHECK_GE(params_.total_num_pixels_covered, 0);

  float inner_distance_to_anchor = kMinPointerDistance / 2;
  float outer_distance_to_anchor =
      inner_distance_to_anchor + params_.total_num_pixels_covered / 2;

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

SyntheticPinchGestureNew::~SyntheticPinchGestureNew() {}

SyntheticGestureNew::Result SyntheticPinchGestureNew::ForwardInputEvents(
    const base::TimeDelta& interval, SyntheticGestureTarget* target) {

  SyntheticGestureParams::GestureSourceType source =
      params_.gesture_source_type;
  if (source == SyntheticGestureParams::DEFAULT_INPUT)
    source = target->GetDefaultSyntheticGestureSourceType();

  if (!target->SupportsSyntheticGestureSourceType(source))
    return SyntheticGestureNew::GESTURE_SOURCE_TYPE_NOT_SUPPORTED_BY_PLATFORM;

  if (source == SyntheticGestureParams::TOUCH_INPUT)
    return ForwardTouchInputEvents(interval, target);
  else
    return SyntheticGestureNew::GESTURE_SOURCE_TYPE_NOT_IMPLEMENTED;
}

SyntheticGestureNew::Result SyntheticPinchGestureNew::ForwardTouchInputEvents(
    const base::TimeDelta& interval, SyntheticGestureTarget* target) {
  if (HasFinished())
    return SyntheticGestureNew::GESTURE_FINISHED;

  if (!started_) {
    touch_event_.PressPoint(params_.anchor.x(), current_y_0_);
    touch_event_.PressPoint(params_.anchor.x(), current_y_1_);
    ForwardTouchEvent(target);
    started_ = true;
  }

  float delta = GetPositionDelta(interval) / 2;
  if (params_.zoom_in) {
    current_y_0_ -= delta;
    current_y_1_ += delta;
  } else {
    current_y_0_ += delta;
    current_y_1_ -= delta;
  }
  touch_event_.MovePoint(0, params_.anchor.x(), current_y_0_);
  touch_event_.MovePoint(1, params_.anchor.x(), current_y_1_);
  ForwardTouchEvent(target);

  if (HasFinished()) {
    touch_event_.ReleasePoint(0);
    touch_event_.ReleasePoint(1);
    ForwardTouchEvent(target);
    return SyntheticGestureNew::GESTURE_FINISHED;
  }

  return SyntheticGestureNew::GESTURE_RUNNING;
}

void SyntheticPinchGestureNew::ForwardTouchEvent(
    SyntheticGestureTarget* target) {
  target->DispatchInputEventToPlatform(
      InputEvent(touch_event_, ui::LatencyInfo(), false));
}

float SyntheticPinchGestureNew::GetPositionDelta(
    const base::TimeDelta& interval) {
  return params_.relative_pointer_speed_in_pixels_s * interval.InSecondsF();
}

bool SyntheticPinchGestureNew::HasFinished() {
  return params_.zoom_in ? (current_y_0_ <= target_y_0_)
                         : (current_y_0_ >= target_y_0_);
}

}  // namespace content
