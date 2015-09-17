// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/perf/random_selector.h"

#include "base/logging.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

RandomSelector::RandomSelector() : sum_of_weights_(0) {}

RandomSelector::~RandomSelector() {}

double RandomSelector::SumWeights(const std::vector<WeightAndValue>& odds) {
  double sum = 0.0;
  for (const auto& odd : odds) {
    sum += odd.weight;
  }
  return sum;
}

void RandomSelector::SetOdds(const std::vector<WeightAndValue>& odds) {
  odds_ = odds;
  sum_of_weights_ = SumWeights(odds_);
}

const std::string& RandomSelector::Select() {
  // Get a random double between 0 and the sum.
  double random = RandDoubleUpTo(sum_of_weights_);
  // Figure out what it belongs to.
  return GetValueFor(random);
}

double RandomSelector::RandDoubleUpTo(double max) {
  CHECK_GT(max, 0.0);
  return max * base::RandDouble();
}

const std::string& RandomSelector::GetValueFor(double random) {
  double current = 0.0;
  for (const auto& odd : odds_) {
    current += odd.weight;
    if (random < current)
      return odd.value;
  }
  NOTREACHED() << "Invalid value for key: " << random;
  return base::EmptyString();
}
