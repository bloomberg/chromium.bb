// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <algorithm>
#include <cmath>

#include "cc/base/rolling_time_delta_history.h"

namespace cc {

RollingTimeDeltaHistory::RollingTimeDeltaHistory(size_t max_size)
    : next_index_(0), max_size_(max_size) {
  sample_vector_.reserve(max_size_);
}

RollingTimeDeltaHistory::~RollingTimeDeltaHistory() {}

void RollingTimeDeltaHistory::InsertSample(base::TimeDelta time) {
  if (max_size_ == 0)
    return;

  if (sample_vector_.size() == max_size_) {
    sample_vector_[next_index_++] = time;
    if (next_index_ == max_size_)
      next_index_ = 0;
  } else {
    sample_vector_.push_back(time);
  }
}

void RollingTimeDeltaHistory::Clear() {
  sample_vector_.clear();
  next_index_ = 0;
}

base::TimeDelta RollingTimeDeltaHistory::Percentile(double percent) const {
  if (sample_vector_.size() == 0)
    return base::TimeDelta();

  double fraction = percent / 100.0;
  if (fraction <= 0.0)
    return *std::min_element(sample_vector_.begin(), sample_vector_.end());

  if (fraction >= 1.0)
    return *std::max_element(sample_vector_.begin(), sample_vector_.end());

  size_t num_smaller_samples =
      static_cast<size_t>(std::ceil(fraction * sample_vector_.size())) - 1;

  std::vector<base::TimeDelta> v(sample_vector_.begin(), sample_vector_.end());
  std::nth_element(v.begin(), v.begin() + num_smaller_samples, v.end());
  return v[num_smaller_samples];
}

}  // namespace cc
