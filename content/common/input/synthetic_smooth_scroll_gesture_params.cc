// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/synthetic_smooth_scroll_gesture_params.h"

#include "base/pickle.h"
#include "content/common/input_messages.h"
#include "ipc/ipc_message_utils.h"

namespace content {

SyntheticSmoothScrollGestureParams::SyntheticSmoothScrollGestureParams()
    : distance(100), anchor_x(0), anchor_y(0) {}

SyntheticSmoothScrollGestureParams::SyntheticSmoothScrollGestureParams(
      const SyntheticSmoothScrollGestureParams& other)
    : SyntheticGestureParams(other),
      distance(other.distance),
      anchor_x(other.anchor_x),
      anchor_y(other.anchor_y) {}

SyntheticSmoothScrollGestureParams::~SyntheticSmoothScrollGestureParams() {}

SyntheticGestureParams::GestureType
SyntheticSmoothScrollGestureParams::GetGestureType() const {
  return SMOOTH_SCROLL_GESTURE;
}

const SyntheticSmoothScrollGestureParams*
SyntheticSmoothScrollGestureParams::Cast(
    const SyntheticGestureParams* gesture_params) {
  DCHECK(gesture_params);
  DCHECK_EQ(SMOOTH_SCROLL_GESTURE, gesture_params->GetGestureType());
  return static_cast<const SyntheticSmoothScrollGestureParams*>(gesture_params);
}

}  // namespace content
