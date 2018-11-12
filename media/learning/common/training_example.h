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

  // Observed feature values.
  std::vector<FeatureValue> features;

  // Observed output value, when given |features| as input.
  TargetValue target_value;

  // Copy / assignment is allowed.
};

COMPONENT_EXPORT(LEARNING_COMMON)
std::ostream& operator<<(std::ostream& out, const TrainingExample& example);

}  // namespace learning
}  // namespace media

#endif  // MEDIA_LEARNING_COMMON_TRAINING_EXAMPLE_H_
