// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/smooth_scroll_calculator.h"

namespace content {

SmoothScrollCalculator::SmoothScrollCalculator() {
}

SmoothScrollCalculator::~SmoothScrollCalculator() {
}

double SmoothScrollCalculator::GetScrollDelta(
    base::TimeTicks now, base::TimeDelta desired_interval) {
  double position_delta = 10;
  if (!last_tick_time_.is_null()) {
    double velocity = 10 / desired_interval.InMillisecondsF();
    double time_delta = (now - last_tick_time_).InMillisecondsF();
    position_delta = velocity * time_delta;
  }

  last_tick_time_ = now;
  return position_delta;
}

}  // namespace content
