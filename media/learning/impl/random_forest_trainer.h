// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_LEARNING_IMPL_RANDOM_FOREST_TRAINER_H_
#define MEDIA_LEARNING_IMPL_RANDOM_FOREST_TRAINER_H_

#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "media/learning/common/learning_task.h"
#include "media/learning/impl/random_number_generator.h"
#include "media/learning/impl/training_algorithm.h"

namespace media {
namespace learning {

// Bagged forest of randomized trees.
// TODO(liberato): consider a generic Bagging class.
class COMPONENT_EXPORT(LEARNING_IMPL) RandomForestTrainer
    : public HasRandomNumberGenerator {
 public:
  RandomForestTrainer();
  ~RandomForestTrainer();

  struct COMPONENT_EXPORT(LEARNING_IMPL) TrainingResult {
    TrainingResult();
    ~TrainingResult();

    std::unique_ptr<Model> model;

    // Number of correctly classified oob samples.
    size_t oob_correct = 0;

    // TODO: include oob entropy and oob unrepresentable?

    // Total number of oob samples.
    size_t oob_total = 0;

    DISALLOW_COPY_AND_ASSIGN(TrainingResult);
  };

  std::unique_ptr<TrainingResult> Train(const LearningTask& task,
                                        const TrainingData& training_data);

 private:
  DISALLOW_COPY_AND_ASSIGN(RandomForestTrainer);
};

}  // namespace learning
}  // namespace media

#endif  // MEDIA_LEARNING_IMPL_RANDOM_FOREST_TRAINER_H_
