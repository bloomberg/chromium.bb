// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/fps_meter.h"

#include <algorithm>

namespace vr_shell {

namespace {

static constexpr size_t kNumFrameTimes = 200;

}  // namepsace

FPSMeter::FPSMeter() : total_time_us_(0) {
  frame_times_.reserve(kNumFrameTimes);
}

FPSMeter::~FPSMeter() {}

void FPSMeter::AddFrame(const base::TimeTicks& time_stamp) {
  if (last_time_stamp_.is_null()) {
    last_time_stamp_ = time_stamp;
    return;
  }

  base::TimeDelta delta = time_stamp - last_time_stamp_;
  last_time_stamp_ = time_stamp;

  total_time_us_ += delta.InMicroseconds();

  if (frame_times_.size() + 1 < kNumFrameTimes) {
    frame_times_.push_back(delta);
  } else {
    total_time_us_ -= frame_times_[current_index_].InMicroseconds();
    frame_times_[current_index_] = delta;
  }

  current_index_++;
  if (current_index_ >= kNumFrameTimes)
    current_index_ = 0;
}

bool FPSMeter::CanComputeFPS() const {
  return !frame_times_.empty();
}

// Simply takes the average time delta.
double FPSMeter::GetFPS() const {
  if (!CanComputeFPS())
    return 0.0;

  return (frame_times_.size() * 1.0e6) / total_time_us_;
}

}  // namespace vr_shell
