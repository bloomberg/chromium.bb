// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/port/browser/smooth_scroll_gesture.h"

namespace content {

double SmoothScrollGesture::Tick(
    base::TimeTicks now, double desired_interval_ms) {
  double position_delta = 10;
  if (!last_tick_time_.is_null()) {
    double velocity = 10 / desired_interval_ms;
    double time_delta = (now - last_tick_time_).InMillisecondsF();
    position_delta = velocity * time_delta;
  }

  last_tick_time_ = now;
  return position_delta;
}

}  // namespace content
