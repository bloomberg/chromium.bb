// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_LEARNING_COMMON_TRAINING_EXAMPLE_H_
#define MEDIA_LEARNING_COMMON_TRAINING_EXAMPLE_H_

#include <initializer_list>
#include <ostream>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "media/learning/common/value.h"

namespace media {
namespace learning {

// Vector of features, for training or prediction.
// To interpret the features, one probably needs to check a LearningTask.  It
// provides a description for each index.  For example, [0]=="height",
// [1]=="url", etc.
using FeatureVector = std::vector<FeatureValue>;

// One training example == group of feature values, plus the desired target.
struct COMPONENT_EXPORT(LEARNING_COMMON) TrainingExample {
  TrainingExample();
  TrainingExample(std::initializer_list<FeatureValue> init_list,
                  TargetValue target);
  TrainingExample(const TrainingExample& rhs);
  TrainingExample(TrainingExample&& rhs) noexcept;
  ~TrainingExample();

  bool operator==(const TrainingExample& rhs) const;
  bool operator!=(const TrainingExample& rhs) const;

  TrainingExample& operator=(const TrainingExample& rhs);
  TrainingExample& operator=(TrainingExample&& rhs);

  // Observed feature values.
  // Note that to interpret these values, you probably need to have the
  // LearningTask that they're supposed to be used with.
  FeatureVector features;

  // Observed output value, when given |features| as input.
  TargetValue target_value;

  // Copy / assignment is allowed.
};

// Collection of training examples.  We use a vector since we allow duplicates.
using TrainingDataStorage = std::vector<TrainingExample>;

// Collection of pointers to training data.  References would be more convenient
// but they're not allowed.
using TrainingData = std::vector<const TrainingExample*>;

COMPONENT_EXPORT(LEARNING_COMMON)
std::ostream& operator<<(std::ostream& out, const TrainingExample& example);

}  // namespace learning
}  // namespace media

#endif  // MEDIA_LEARNING_COMMON_TRAINING_EXAMPLE_H_
