// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/fps_meter.h"

#include <algorithm>

namespace vr_shell {

namespace {

static constexpr size_t kDefaultNumFrameTimes = 10;

}  // namespace

SampleQueue::SampleQueue(size_t window_size) : window_size_(window_size) {
  samples_.reserve(window_size);
}

SampleQueue::~SampleQueue() = default;

void SampleQueue::AddSample(int64_t value) {
  sum_ += value;

  if (samples_.size() < window_size_) {
    samples_.push_back(value);
  } else {
    sum_ -= samples_[current_index_];
    samples_[current_index_] = value;
  }

  ++current_index_;
  if (current_index_ >= window_size_) {
    current_index_ = 0;
  }
}

FPSMeter::FPSMeter() : frame_times_(kDefaultNumFrameTimes) {}

FPSMeter::FPSMeter(size_t window_size) : frame_times_(window_size) {}

FPSMeter::~FPSMeter() = default;

size_t FPSMeter::GetNumFrameTimes() {
  return frame_times_.GetWindowSize();
}

void FPSMeter::AddFrame(const base::TimeTicks& time_stamp) {
  if (last_time_stamp_.is_null()) {
    last_time_stamp_ = time_stamp;
    return;
  }

  int64_t delta = (time_stamp - last_time_stamp_).InMicroseconds();
  last_time_stamp_ = time_stamp;

  frame_times_.AddSample(delta);
}

bool FPSMeter::CanComputeFPS() const {
  return frame_times_.GetCount() > 0;
}

// Simply takes the average time delta.
double FPSMeter::GetFPS() const {
  if (!CanComputeFPS())
    return 0.0;

  return (frame_times_.GetCount() * 1.0e6) / frame_times_.GetSum();
}

SlidingAverage::SlidingAverage(size_t window_size) : values_(window_size) {}

SlidingAverage::~SlidingAverage() = default;

void SlidingAverage::AddSample(int64_t value) {
  values_.AddSample(value);
}

int64_t SlidingAverage::GetAverageOrDefault(int64_t default_value) const {
  if (values_.GetCount() == 0)
    return default_value;
  return values_.GetSum() / values_.GetCount();
}

}  // namespace vr_shell
