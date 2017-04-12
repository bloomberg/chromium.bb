// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_FPS_METER_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_FPS_METER_H_

#include <vector>

#include "base/macros.h"
#include "base/time/time.h"

namespace vr_shell {

// Computes fps based on submitted frame times.
class FPSMeter {
 public:
  FPSMeter();
  ~FPSMeter();

  void AddFrame(const base::TimeTicks& time_stamp);

  bool CanComputeFPS() const;

  double GetFPS() const;

 private:
  size_t current_index_;
  int64_t total_time_us_;
  base::TimeTicks last_time_stamp_;
  std::vector<base::TimeDelta> frame_times_;
  DISALLOW_COPY_AND_ASSIGN(FPSMeter);
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_FPS_METER_H_
