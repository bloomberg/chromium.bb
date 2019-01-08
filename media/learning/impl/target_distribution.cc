// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/impl/target_distribution.h"

#include <sstream>

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

TargetDistribution& TargetDistribution::operator+=(
    const LabelledExample& example) {
  counts_[example.target_value] += example.weight;
  return *this;
}

size_t TargetDistribution::operator[](const TargetValue& value) const {
  auto iter = counts_.find(value);
  if (iter == counts_.end())
    return 0;

  return iter->second;
}

size_t& TargetDistribution::operator[](const TargetValue& value) {
  return counts_[value];
}

bool TargetDistribution::FindSingularMax(TargetValue* value_out,
                                         size_t* counts_out) const {
  if (!counts_.size())
    return false;

  size_t unused_counts;
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

double TargetDistribution::Average() const {
  double total_value = 0.;
  size_t total_counts = 0;
  for (auto& iter : counts_) {
    total_value += iter.first.value() * iter.second;
    total_counts += iter.second;
  }

  if (!total_counts)
    return 0.;

  return total_value / total_counts;
}

std::string TargetDistribution::ToString() const {
  std::ostringstream ss;
  ss << "[";
  for (auto& entry : counts_)
    ss << " " << entry.first << ":" << entry.second;
  ss << " ]";

  return ss.str();
}

std::ostream& operator<<(std::ostream& out,
                         const media::learning::TargetDistribution& dist) {
  return out << dist.ToString();
}

}  // namespace learning
}  // namespace media
