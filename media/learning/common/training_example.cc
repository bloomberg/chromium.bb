// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/common/training_example.h"

namespace media {
namespace learning {

TrainingExample::TrainingExample() = default;

TrainingExample::TrainingExample(std::initializer_list<FeatureValue> init_list,
                                 TargetValue target)
    : features(init_list), target_value(target) {}

TrainingExample::TrainingExample(const TrainingExample& rhs)
    : features(rhs.features), target_value(rhs.target_value) {}

TrainingExample::TrainingExample(TrainingExample&& rhs) noexcept
    : features(std::move(rhs.features)),
      target_value(std::move(rhs.target_value)) {}

TrainingExample::~TrainingExample() = default;

std::ostream& operator<<(std::ostream& out, const TrainingExample& example) {
  for (const auto& feature : example.features)
    out << " " << feature;

  out << " => " << example.target_value;

  return out;
}

bool TrainingExample::operator==(const TrainingExample& rhs) const {
  return target_value == rhs.target_value && features == rhs.features;
}

bool TrainingExample::operator!=(const TrainingExample& rhs) const {
  return !((*this) == rhs);
}

bool TrainingExample::operator<(const TrainingExample& rhs) const {
  // Impose a somewhat arbitrary ordering.
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

TrainingDataStorage::TrainingDataStorage() = default;

TrainingDataStorage::~TrainingDataStorage() = default;

WeightedExample::WeightedExample(const TrainingExample* example,
                                 weight_t weight)
    : example_(example), weight_(weight) {}

WeightedExample::~WeightedExample() = default;

TrainingData::TrainingData(scoped_refptr<TrainingDataStorage> backing_storage)
    : backing_storage_(std::move(backing_storage)) {}

TrainingData::TrainingData(scoped_refptr<TrainingDataStorage> backing_storage,
                           TrainingDataStorage::const_iterator begin,
                           TrainingDataStorage::const_iterator end)
    : backing_storage_(std::move(backing_storage)) {
  for (; begin != end; begin++) {
    examples_.push_back(*begin);
    total_weight_ += begin->weight();
  }
}

TrainingData::TrainingData(const TrainingData& rhs) = default;

TrainingData::TrainingData(TrainingData&& rhs) = default;

TrainingData::~TrainingData() = default;

}  // namespace learning
}  // namespace media
