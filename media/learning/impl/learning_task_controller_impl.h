// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_LEARNING_IMPL_LEARNING_TASK_CONTROLLER_IMPL_H_
#define MEDIA_LEARNING_IMPL_LEARNING_TASK_CONTROLLER_IMPL_H_

#include <memory>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "media/learning/impl/distribution_reporter.h"
#include "media/learning/impl/learning_task_controller.h"
#include "media/learning/impl/training_algorithm.h"

namespace media {
namespace learning {

class LearningTaskControllerImplTest;

class COMPONENT_EXPORT(LEARNING_IMPL) LearningTaskControllerImpl
    : public LearningTaskController,
      public base::SupportsWeakPtr<LearningTaskControllerImpl> {
 public:
  LearningTaskControllerImpl(
      const LearningTask& task,
      std::unique_ptr<DistributionReporter> reporter = nullptr);
  ~LearningTaskControllerImpl() override;

  // LearningTaskController
  void AddExample(const LabelledExample& example) override;

 private:
  // Override the training CB for testing.
  void SetTrainingCBForTesting(TrainingAlgorithmCB cb);

  // Called by |training_cb_| when the model is trained.
  void OnModelTrained(std::unique_ptr<Model> model);

  LearningTask task_;

  // Current batch of examples.
  std::unique_ptr<TrainingData> training_data_;

  // Most recently trained model, or null.
  std::unique_ptr<Model> model_;

  TrainingAlgorithmCB training_cb_;

  // Optional reporter for training accuracy.
  std::unique_ptr<DistributionReporter> reporter_;

  friend class LearningTaskControllerImplTest;
};

}  // namespace learning
}  // namespace media

#endif  // MEDIA_LEARNING_IMPL_LEARNING_TASK_CONTROLLER_IMPL_H_
