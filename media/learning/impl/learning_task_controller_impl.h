// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_LEARNING_IMPL_LEARNING_TASK_CONTROLLER_IMPL_H_
#define MEDIA_LEARNING_IMPL_LEARNING_TASK_CONTROLLER_IMPL_H_

#include <memory>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "media/learning/impl/distribution_reporter.h"
#include "media/learning/impl/feature_provider.h"
#include "media/learning/impl/learning_task_controller.h"
#include "media/learning/impl/random_number_generator.h"
#include "media/learning/impl/training_algorithm.h"

namespace media {
namespace learning {

class LearningTaskControllerImplTest;

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
  void AddExample(const LabelledExample& example) override;

 private:
  // Trampoline method for receiving examples from |feature_provider_|.  Will
  // chain directly to OnExampleReady if we're on |task_runner| and |weak_this|
  // is non-null, else it will post to |task_runner|.
  static void OnFeaturesReadyTrampoline(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      base::WeakPtr<LearningTaskControllerImpl> weak_this,
      LabelledExample example,
      FeatureVector features);

  // Called when a new feature vector has been finished by |feature_provider_|,
  // if needed, to actually add the example.  |features| will replace the ones
  // in |example|.
  void OnFeaturesReady(LabelledExample example, FeatureVector features);

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

  // Optional feature provider.
  SequenceBoundFeatureProvider feature_provider_;

  // Optional reporter for training accuracy.
  std::unique_ptr<DistributionReporter> reporter_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  friend class LearningTaskControllerImplTest;
};

}  // namespace learning
}  // namespace media

#endif  // MEDIA_LEARNING_IMPL_LEARNING_TASK_CONTROLLER_IMPL_H_
