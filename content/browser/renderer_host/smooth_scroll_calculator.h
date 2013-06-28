// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_SMOOTH_SCROLL_CALCULATOR_H_
#define CONTENT_BROWSER_RENDERER_HOST_SMOOTH_SCROLL_CALCULATOR_H_

#include "base/time/time.h"

namespace content {

// An utility class to calculate the delta for smooth scroll gesture
// events.
class SmoothScrollCalculator {
 public:
  SmoothScrollCalculator();
  ~SmoothScrollCalculator();

  double GetScrollDelta(base::TimeTicks now, base::TimeDelta desired_interval);

 private:
  base::TimeTicks last_tick_time_;

  DISALLOW_COPY_AND_ASSIGN(SmoothScrollCalculator);
};

} // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_SMOOTH_SCROLL_CALCULATOR_H_
