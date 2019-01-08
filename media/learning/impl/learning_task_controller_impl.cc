// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/impl/learning_task_controller_impl.h"

#include <memory>

#include "base/bind.h"
#include "media/learning/impl/extra_trees_trainer.h"
#include "media/learning/impl/random_tree_trainer.h"

namespace media {
namespace learning {

LearningTaskControllerImpl::LearningTaskControllerImpl(const LearningTask& task)
    : task_(task), training_data_(std::make_unique<TrainingData>()) {
  switch (task_.model) {
    case LearningTask::Model::kExtraTrees:
      training_cb_ = base::BindRepeating(
          [](const LearningTask& task, TrainingData training_data,
             TrainedModelCB model_cb) {
            ExtraTreesTrainer trainer;
            std::move(model_cb).Run(trainer.Train(task, training_data));
          },
          task_);
      break;
    case LearningTask::Model::kRandomForest:
      // TODO(liberato): forest!
      training_cb_ = RandomTreeTrainer::GetTrainingAlgorithmCB(task_);
      break;
  }

  // TODO(liberato): Record via UMA based on the task name.
  accuracy_reporting_cb_ =
      base::BindRepeating([](const LearningTask&, bool is_correct) {});
}

LearningTaskControllerImpl::~LearningTaskControllerImpl() = default;

void LearningTaskControllerImpl::AddExample(const LabelledExample& example) {
  // TODO(liberato): do we ever trim older examples?
  training_data_->push_back(example);

  // Once we have a model, see if we'd get |example| correct.
  if (model_) {
    TargetDistribution distribution =
        model_->PredictDistribution(example.features);

    TargetValue predicted_value;
    const bool is_correct = distribution.FindSingularMax(&predicted_value) &&
                            predicted_value == example.target_value;
    accuracy_reporting_cb_.Run(task_, is_correct);
    // TODO(liberato): record entropy / not representable?
  }

  // Train every time we get a multiple of |data_set_size|.
  // TODO(liberato): weight might go up by more than one.
  if ((training_data_->total_weight() % task_.min_data_set_size) != 0)
    return;

  TrainedModelCB model_cb =
      base::BindOnce(&LearningTaskControllerImpl::OnModelTrained, AsWeakPtr());
  // TODO(liberato): Post to a background task runner.
  training_cb_.Run(*training_data_.get(), std::move(model_cb));

  // TODO(liberato): replace |training_data_| and merge them once the model is
  // trained.  Else, new examples will change the data during training.  For
  // now, training is synchronous, so it's okay as it is.
}

void LearningTaskControllerImpl::OnModelTrained(std::unique_ptr<Model> model) {
  model_ = std::move(model);
  // TODO(liberato): record oob results.
}

void LearningTaskControllerImpl::SetTrainingCBForTesting(
    TrainingAlgorithmCB cb) {
  training_cb_ = std::move(cb);
}

void LearningTaskControllerImpl::SetAccuracyReportingCBForTesting(
    AccuracyReportingCB cb) {
  accuracy_reporting_cb_ = std::move(cb);
}

}  // namespace learning
}  // namespace media
