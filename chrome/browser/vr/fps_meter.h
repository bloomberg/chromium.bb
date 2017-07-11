// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_FPS_METER_H_
#define CHROME_BROWSER_VR_FPS_METER_H_

#include <vector>

#include "base/macros.h"
#include "base/time/time.h"

namespace vr {

class SampleQueue {
 public:
  explicit SampleQueue(size_t window_size);
  ~SampleQueue();

  int64_t GetSum() const { return sum_; }

  void AddSample(int64_t value);

  size_t GetCount() const { return samples_.size(); }

  // Get sliding window size for tests.
  size_t GetWindowSize() const { return window_size_; }

 private:
  int64_t sum_ = 0;
  size_t current_index_ = 0;
  size_t window_size_;
  std::vector<int64_t> samples_;
};

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

class SlidingAverage {
 public:
  explicit SlidingAverage(size_t window_size);
  ~SlidingAverage();

  void AddSample(int64_t value);
  int64_t GetAverageOrDefault(int64_t default_value) const;
  int64_t GetAverage() const { return GetAverageOrDefault(0); }

 private:
  SampleQueue values_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_FPS_METER_H_
