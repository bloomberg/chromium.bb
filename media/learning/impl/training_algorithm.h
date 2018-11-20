// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_LEARNING_IMPL_TRAINING_ALGORITHM_H_
#define MEDIA_LEARNING_IMPL_TRAINING_ALGORITHM_H_

#include <memory>

#include "base/callback.h"
#include "media/learning/common/training_example.h"
#include "media/learning/impl/model.h"

namespace media {
namespace learning {

// A TrainingAlgorithm takes as input training examples, and produces as output
// a trained model that can be used for prediction.
// Train a model with on |examples| and return it.
// TODO(liberato): Switch to a callback to return the model.
using TrainingAlgorithmCB = base::RepeatingCallback<std::unique_ptr<Model>(
    const TrainingData& examples)>;

}  // namespace learning
}  // namespace media

#endif  // MEDIA_LEARNING_IMPL_TRAINING_ALGORITHM_H_
