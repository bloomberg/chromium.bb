// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_LEARNING_IMPL_LEARNER_H_
#define MEDIA_LEARNING_IMPL_LEARNER_H_

#include "base/component_export.h"
#include "base/values.h"
#include "media/learning/common/instance.h"

namespace media {
namespace learning {

// Base class for a thing that takes examples of the form {features, target},
// and trains a model to predict the target given the features.  The target may
// be either nominal (classification) or numeric (regression), though this must
// be chosen in advance when creating the learner via LearnerFactory.
class COMPONENT_EXPORT(LEARNING_IMPL) Learner {
 public:
  virtual ~Learner() = default;

  // Tell the learner that |instance| has been observed with the target value
  // |target| during training.
  virtual void AddExample(const Instance& instance,
                          const TargetValue& target) = 0;
};

}  // namespace learning
}  // namespace media

#endif  // MEDIA_LEARNING_IMPL_LEARNER_H_
