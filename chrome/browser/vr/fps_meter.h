// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_FPS_METER_H_
#define CHROME_BROWSER_VR_FPS_METER_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/vr/sample_queue.h"

namespace vr {

// Computes fps based on submitted frame times.
class FPSMeter {
 public:
  FPSMeter();
  explicit FPSMeter(size_t window_size);
  ~FPSMeter();

  void AddFrame(const base::TimeTicks& time_stamp);

  bool CanComputeFPS() const;

  double GetFPS() const;

  // Get sliding window size for tests.
  size_t GetNumFrameTimes();

 private:
  SampleQueue frame_times_;
  base::TimeTicks last_time_stamp_;
  DISALLOW_COPY_AND_ASSIGN(FPSMeter);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_FPS_METER_H_
