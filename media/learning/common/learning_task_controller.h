// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_LEARNING_COMMON_LEARNING_TASK_CONTROLLER_H_
#define MEDIA_LEARNING_COMMON_LEARNING_TASK_CONTROLLER_H_

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "media/learning/common/labelled_example.h"
#include "media/learning/common/learning_task.h"

namespace media {
namespace learning {

// Client for a single learning task.  Intended to be the primary API for client
// code that generates FeatureVectors / requests predictions for a single task.
// The API supports sending in an observed FeatureVector without a target value,
// so that framework-provided features (FeatureProvider) can be snapshotted at
// the right time.  One doesn't generally want to wait until the TargetValue is
// observed to do that.
class COMPONENT_EXPORT(LEARNING_COMMON) LearningTaskController {
 public:
  LearningTaskController() = default;
  virtual ~LearningTaskController() = default;

  // Used to set the target value and example weight.
  using SetTargetValueCB = base::OnceCallback<void(TargetValue, WeightType)>;

  // Record a FeatureVector that may be used for prediction and / or adding a
  // new example.  Call this at the time one would try to predict the
  // TargetValue.  This lets the framework snapshot any framework-provided
  // feature values at prediction time.  Later, if you want to turn these
  // features into an example for training a model, then call the returned CB
  // with the TargetValue and weight.  Otherwise, you may discard the CB.
  // TODO(liberato): This should optionally take a callback to receive a
  // prediction for the FeatureVector.
  // TODO(liberato): See if this ends up generating smaller code with pass-by-
  // value or with |FeatureVector&&|, once we have callers that can actually
  // benefit from it.
  virtual SetTargetValueCB BeginObservation(const FeatureVector& features) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(LearningTaskController);
};

}  // namespace learning
}  // namespace media

#endif  // MEDIA_LEARNING_COMMON_LEARNING_TASK_CONTROLLER_H_
