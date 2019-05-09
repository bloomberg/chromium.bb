// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_METRICS_TIME_TO_FIRST_PRESENT_RECORDER_H_
#define ASH_METRICS_TIME_TO_FIRST_PRESENT_RECORDER_H_

#include <stdint.h>

#include "ash/ash_export.h"
#include "base/callback.h"
#include "base/macros.h"

namespace aura {
class Window;
}

namespace gfx {
struct PresentationFeedback;
}

namespace ash {

class TimeToFirstPresentRecorderTestApi;

// Used for tracking the time main started to the time the first bits make it
// the screen and logging a histogram of the time.
//
// This only logs the time to present the primary root window.
class ASH_EXPORT TimeToFirstPresentRecorder {
 public:
  // The name of the histogram the time is logged against.
  static const char kMetricName[];

  explicit TimeToFirstPresentRecorder(aura::Window* window);
  ~TimeToFirstPresentRecorder();

 private:
  friend class TimeToFirstPresentRecorderTestApi;

  // Callback from the compositor when it presented a valid frame.
  void DidPresentCompositorFrame(const gfx::PresentationFeedback& feedback);

  // Only used by tests. If valid it's Run() when the time is recorded.
  base::OnceClosure log_callback_;

  DISALLOW_COPY_AND_ASSIGN(TimeToFirstPresentRecorder);
};

}  // namespace ash

#endif  // ASH_METRICS_TIME_TO_FIRST_PRESENT_RECORDER_H_
