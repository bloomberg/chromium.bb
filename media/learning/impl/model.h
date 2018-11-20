// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_LEARNING_IMPL_MODEL_H_
#define MEDIA_LEARNING_IMPL_MODEL_H_

#include <map>

#include "base/component_export.h"
#include "media/learning/common/training_example.h"
#include "media/learning/impl/model.h"

namespace media {
namespace learning {

// One trained model, useful for making predictions.
// TODO(liberato): Provide an API for incremental update, for those models that
// can support it.
class COMPONENT_EXPORT(LEARNING_IMPL) Model {
 public:
  // [target value] == counts
  // This is classification-centric.  Not sure about the right interface for
  // regressors.  Mostly for testing.
  using TargetDistribution = std::map<TargetValue, int>;

  virtual ~Model() = default;

  virtual TargetDistribution PredictDistribution(
      const FeatureVector& instance) = 0;
};

}  // namespace learning
}  // namespace media

#endif  // MEDIA_LEARNING_IMPL_MODEL_H_
