// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/common/training_example.h"

#include "base/containers/flat_set.h"

namespace media {
namespace learning {

TrainingExample::TrainingExample() = default;

TrainingExample::TrainingExample(std::initializer_list<FeatureValue> init_list,
                                 TargetValue target)
    : features(init_list), target_value(target) {}

TrainingExample::TrainingExample(const TrainingExample& rhs) = default;

TrainingExample::TrainingExample(TrainingExample&& rhs) noexcept = default;

TrainingExample::~TrainingExample() = default;

std::ostream& operator<<(std::ostream& out, const TrainingExample& example) {
  out << example.features << " => " << example.target_value;

  return out;
}

std::ostream& operator<<(std::ostream& out, const FeatureVector& features) {
  for (const auto& feature : features)
    out << " " << feature;

  return out;
}

bool TrainingExample::operator==(const TrainingExample& rhs) const {
  // Do not check weight.
  return target_value == rhs.target_value && features == rhs.features;
}

bool TrainingExample::operator!=(const TrainingExample& rhs) const {
  // Do not check weight.
  return !((*this) == rhs);
}

bool TrainingExample::operator<(const TrainingExample& rhs) const {
  // Impose a somewhat arbitrary ordering.
  // Do not check weight.
  if (target_value != rhs.target_value)
    return target_value < rhs.target_value;

  // Note that we could short-circuit this if the feature vector lengths are
  // unequal, since we don't particularly care how they compare as long as it's
  // stable.  In particular, we don't have any notion of a "prefix".
  return features < rhs.features;
}

TrainingExample& TrainingExample::operator=(const TrainingExample& rhs) =
    default;

TrainingExample& TrainingExample::operator=(TrainingExample&& rhs) = default;

TrainingData::TrainingData() = default;

TrainingData::TrainingData(const TrainingData& rhs) = default;

TrainingData::TrainingData(TrainingData&& rhs) = default;

TrainingData::~TrainingData() = default;

TrainingData TrainingData::DeDuplicate() const {
  // flat_set has non-const iterators, while std::set does not.  const_cast is
  // not allowed by chromium style outside of getters, so flat_set it is.
  base::flat_set<TrainingExample> example_set;
  for (auto& example : examples_) {
    auto iter = example_set.find(example);
    if (iter != example_set.end())
      iter->weight += example.weight;
    else
      example_set.insert(example);
  }

  TrainingData deduplicated_data;
  for (auto& example : example_set)
    deduplicated_data.push_back(example);

  return deduplicated_data;
}

}  // namespace learning
}  // namespace media
