// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/synthetic_gesture_calculator.h"


namespace {

const float kDefaultPositionDelta = 10.0f;

}


namespace content {

SyntheticGestureCalculator::SyntheticGestureCalculator() {
}

SyntheticGestureCalculator::~SyntheticGestureCalculator() {
}

float SyntheticGestureCalculator::GetDelta(
    base::TimeTicks now, base::TimeDelta desired_interval) {
  float position_delta = kDefaultPositionDelta;
  if (!last_tick_time_.is_null()) {
    float velocity = kDefaultPositionDelta /
        (float)desired_interval.InMillisecondsF();
    float time_delta = (now - last_tick_time_).InMillisecondsF();
    position_delta = velocity * time_delta;
  }

  last_tick_time_ = now;
  return position_delta;
}

}  // namespace content
