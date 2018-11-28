// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/impl/target_distribution.h"

namespace media {
namespace learning {

TargetDistribution::TargetDistribution() = default;

TargetDistribution::TargetDistribution(const TargetDistribution& rhs) = default;

TargetDistribution::TargetDistribution(TargetDistribution&& rhs) = default;

TargetDistribution::~TargetDistribution() = default;

TargetDistribution& TargetDistribution::operator=(
    const TargetDistribution& rhs) = default;

TargetDistribution& TargetDistribution::operator=(TargetDistribution&& rhs) =
    default;

bool TargetDistribution::operator==(const TargetDistribution& rhs) const {
  return rhs.total_counts() == total_counts() && rhs.counts_ == counts_;
}

TargetDistribution& TargetDistribution::operator+=(
    const TargetDistribution& rhs) {
  for (auto& rhs_pair : rhs.counts())
    counts_[rhs_pair.first] += rhs_pair.second;

  return *this;
}

TargetDistribution& TargetDistribution::operator+=(const TargetValue& rhs) {
  counts_[rhs]++;
  return *this;
}

int TargetDistribution::operator[](const TargetValue& value) const {
  auto iter = counts_.find(value);
  if (iter == counts_.end())
    return 0;

  return iter->second;
}

int& TargetDistribution::operator[](const TargetValue& value) {
  return counts_[value];
}

bool TargetDistribution::FindSingularMax(TargetValue* value_out,
                                         int* counts_out) const {
  if (!counts_.size())
    return false;

  int unused_counts;
  if (!counts_out)
    counts_out = &unused_counts;

  auto iter = counts_.begin();
  *value_out = iter->first;
  *counts_out = iter->second;
  bool singular_max = true;
  for (iter++; iter != counts_.end(); iter++) {
    if (iter->second > *counts_out) {
      *value_out = iter->first;
      *counts_out = iter->second;
      singular_max = true;
    } else if (iter->second == *counts_out) {
      // If this turns out to be the max, then it's not singular.
      singular_max = false;
    }
  }

  return singular_max;
}

}  // namespace learning
}  // namespace media
