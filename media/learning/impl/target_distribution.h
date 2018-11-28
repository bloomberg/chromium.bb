// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_LEARNING_IMPL_TARGET_DISTRIBUTION_H_
#define MEDIA_LEARNING_IMPL_TARGET_DISTRIBUTION_H_

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "media/learning/common/value.h"

namespace media {
namespace learning {

// TargetDistribution of target values.
class COMPONENT_EXPORT(LEARNING_IMPL) TargetDistribution {
 public:
  TargetDistribution();
  ~TargetDistribution();

  // Add |rhs| to our counts.
  TargetDistribution& operator+=(const TargetDistribution& rhs);

  // Increment |rhs| by one.
  TargetDistribution& operator+=(const TargetValue& rhs);

  // Return the number of counts for |value|.
  int operator[](const TargetValue& value) const;
  int& operator[](const TargetValue& value);

  // Return the total counts in the map.
  int total_counts() const {
    size_t total = 0u;
    for (auto& entry : counts_)
      total += entry.second;
    return total;
  }

  // Find the singular value with the highest counts, and copy it into
  // |value_out| and (optionally) |counts_out|.  Returns true if there is a
  // singular maximum, else returns false with the out params undefined.
  bool FindSingularMax(TargetValue* value_out, int* counts_out = nullptr) const;

 private:
  // We use a flat_map since this will often have only one or two TargetValues,
  // such as "true" or "false".
  using distribution_map_t = base::flat_map<TargetValue, int>;

  const distribution_map_t& counts() const { return counts_; }

  // [value] == counts
  distribution_map_t counts_;

  // Allow copy and assign.
};

}  // namespace learning
}  // namespace media

#endif  // MEDIA_LEARNING_IMPL_TARGET_DISTRIBUTION_H_
