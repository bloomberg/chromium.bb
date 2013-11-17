// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/synthetic_pinch_gesture_params.h"

#include "base/logging.h"

namespace content {

SyntheticPinchGestureParams::SyntheticPinchGestureParams()
    : zoom_in(true),
      total_num_pixels_covered(100),
      relative_pointer_speed_in_pixels_s(500) {}

SyntheticPinchGestureParams::SyntheticPinchGestureParams(
    const SyntheticPinchGestureParams& other)
    : SyntheticGestureParams(other),
      zoom_in(other.zoom_in),
      total_num_pixels_covered(other.total_num_pixels_covered),
      anchor(other.anchor),
      relative_pointer_speed_in_pixels_s(
          other.relative_pointer_speed_in_pixels_s) {}

SyntheticPinchGestureParams::~SyntheticPinchGestureParams() {}

SyntheticGestureParams::GestureType
SyntheticPinchGestureParams::GetGestureType() const {
  return PINCH_GESTURE;
}

const SyntheticPinchGestureParams* SyntheticPinchGestureParams::Cast(
    const SyntheticGestureParams* gesture_params) {
  DCHECK(gesture_params);
  DCHECK_EQ(PINCH_GESTURE, gesture_params->GetGestureType());
  return static_cast<const SyntheticPinchGestureParams*>(gesture_params);
}

}  // namespace content
