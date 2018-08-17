// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/statistics/weighted_mean.h"

#include <cmath>

namespace chromecast {

WeightedMean::WeightedMean()
    : weighted_mean_(0),
      variance_sum_(0),
      sum_weights_(0),
      sum_squared_weights_(0) {}

void WeightedMean::AddSample(int64_t value, double weight) {
  double old_sum_weights = sum_weights_;
  sum_weights_ += weight;
  // Use std::abs() to handle negative weights (ie, removing a sample).
  sum_squared_weights_ += weight * std::abs(weight);
  if (sum_weights_ == 0) {
    weighted_mean_ = 0;
    variance_sum_ = 0;
  } else {
    double delta = value - weighted_mean_;
    double mean_change = delta * weight / sum_weights_;
    weighted_mean_ += mean_change;
    variance_sum_ += old_sum_weights * delta * mean_change;
  }
}

}  // namespace chromecast
