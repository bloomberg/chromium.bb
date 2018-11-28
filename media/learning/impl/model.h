// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_LEARNING_IMPL_MODEL_H_
#define MEDIA_LEARNING_IMPL_MODEL_H_

#include "base/component_export.h"
#include "media/learning/common/training_example.h"
#include "media/learning/impl/model.h"
#include "media/learning/impl/target_distribution.h"

namespace media {
namespace learning {

// One trained model, useful for making predictions.
// TODO(liberato): Provide an API for incremental update, for those models that
// can support it.
class COMPONENT_EXPORT(LEARNING_IMPL) Model {
 public:
  virtual ~Model() = default;

  virtual TargetDistribution PredictDistribution(
      const FeatureVector& instance) = 0;
};

}  // namespace learning
}  // namespace media

#endif  // MEDIA_LEARNING_IMPL_MODEL_H_
