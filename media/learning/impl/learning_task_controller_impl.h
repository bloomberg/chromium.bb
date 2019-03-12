// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_LEARNING_IMPL_LEARNING_TASK_CONTROLLER_IMPL_H_
#define MEDIA_LEARNING_IMPL_LEARNING_TASK_CONTROLLER_IMPL_H_

#include <memory>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "media/learning/common/learning_task_controller.h"
#include "media/learning/impl/distribution_reporter.h"
#include "media/learning/impl/feature_provider.h"
#include "media/learning/impl/learning_task_controller_helper.h"
#include "media/learning/impl/random_number_generator.h"
#include "media/learning/impl/training_algorithm.h"

namespace media {
namespace learning {

class LearningTaskControllerImplTest;

// Controller for a single learning task.  Takes training examples, and forwards
// them to the learner(s).  Responsible for things like:
//  - Managing underlying learner(s) based on the learning task
//  - Feature subset selection
//  - UMA reporting on accuracy / feature importance
//
// The idea is that one can create a LearningTask, give it to an LTCI, and the
// LTCI will do the work of building / evaluating the model based on training
// examples that are provided to it.
class COMPONENT_EXPORT(LEARNING_IMPL) LearningTaskControllerImpl
    : public LearningTaskController,
      public HasRandomNumberGenerator,
      public base::SupportsWeakPtr<LearningTaskControllerImpl> {
 public:
  LearningTaskControllerImpl(
      const LearningTask& task,
      std::unique_ptr<DistributionReporter> reporter = nullptr,
      SequenceBoundFeatureProvider feature_provider =
          SequenceBoundFeatureProvider());
  ~LearningTaskControllerImpl() override;

  // LearningTaskController
  void BeginObservation(base::UnguessableToken id,
                        const FeatureVector& features) override;
  void CompleteObservation(base::UnguessableToken id,
                           const ObservationCompletion& completion) override;
  void CancelObservation(base::UnguessableToken id) override;

 private:
  // Add |example| to the training data, and process it.
  void AddFinishedExample(LabelledExample example);

  // Called by |training_cb_| when the model is trained.
  void OnModelTrained(std::unique_ptr<Model> model);

  void SetTrainerForTesting(std::unique_ptr<TrainingAlgorithm> trainer);

  LearningTask task_;

  // Current batch of examples.
  std::unique_ptr<TrainingData> training_data_;

  // Most recently trained model, or null.
  std::unique_ptr<Model> model_;

  // We don't want to have multiple models in flight.
  bool training_is_in_progress_ = false;

  // Number of examples in |training_data_| that haven't been used for training.
  // This helps us decide when to train a new model.
  int num_untrained_examples_ = 0;

  // Training algorithm that we'll use.
  std::unique_ptr<TrainingAlgorithm> trainer_;

  // Optional reporter for training accuracy.
  std::unique_ptr<DistributionReporter> reporter_;

  // Helper that we use to handle deferred examples.
  std::unique_ptr<LearningTaskControllerHelper> helper_;

  friend class LearningTaskControllerImplTest;
};

}  // namespace learning
}  // namespace media

#endif  // MEDIA_LEARNING_IMPL_LEARNING_TASK_CONTROLLER_IMPL_H_
