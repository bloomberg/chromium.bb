// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/impl/learning_task_controller_impl.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "media/learning/impl/extra_trees_trainer.h"
#include "media/learning/impl/lookup_table_trainer.h"

namespace media {
namespace learning {

LearningTaskControllerImpl::LearningTaskControllerImpl(
    const LearningTask& task,
    std::unique_ptr<DistributionReporter> reporter,
    SequenceBoundFeatureProvider feature_provider)
    : task_(task),
      training_data_(std::make_unique<TrainingData>()),
      reporter_(std::move(reporter)),
      helper_(std::make_unique<LearningTaskControllerHelper>(
          task,
          base::BindRepeating(&LearningTaskControllerImpl::AddFinishedExample,
                              AsWeakPtr()),
          std::move(feature_provider))) {
  switch (task_.model) {
    case LearningTask::Model::kExtraTrees:
      trainer_ = std::make_unique<ExtraTreesTrainer>();
      break;
    case LearningTask::Model::kLookupTable:
      trainer_ = std::make_unique<LookupTableTrainer>();
      break;
  }
}

LearningTaskControllerImpl::~LearningTaskControllerImpl() = default;

void LearningTaskControllerImpl::BeginObservation(
    base::UnguessableToken id,
    const FeatureVector& features) {
  helper_->BeginObservation(id, features);
}

void LearningTaskControllerImpl::CompleteObservation(
    base::UnguessableToken id,
    const ObservationCompletion& completion) {
  helper_->CompleteObservation(id, completion);
}

void LearningTaskControllerImpl::CancelObservation(base::UnguessableToken id) {
  helper_->CancelObservation(id);
}

void LearningTaskControllerImpl::AddFinishedExample(LabelledExample example) {
  if (training_data_->size() >= task_.max_data_set_size) {
    // Replace a random example.  We don't necessarily want to replace the
    // oldest, since we don't necessarily want to enforce an ad-hoc recency
    // constraint here.  That's a different issue.
    (*training_data_)[rng()->Generate(training_data_->size())] = example;
  } else {
    training_data_->push_back(example);
  }
  // Either way, we have one more example that we haven't used for training yet.
  num_untrained_examples_++;

  // Once we have a model, see if we'd get |example| correct.
  if (model_ && reporter_) {
    TargetDistribution predicted =
        model_->PredictDistribution(example.features);

    TargetDistribution observed;
    observed += example.target_value;
    reporter_->GetPredictionCallback(observed).Run(predicted);
  }

  // Can't train more than one model concurrently.
  if (training_is_in_progress_)
    return;

  // Train every time we get enough new examples.  Note that this works even if
  // we are replacing old examples rather than adding new ones.
  double frac = ((double)num_untrained_examples_) / training_data_->size();
  if (frac < task_.min_new_data_fraction)
    return;

  num_untrained_examples_ = 0;

  TrainedModelCB model_cb =
      base::BindOnce(&LearningTaskControllerImpl::OnModelTrained, AsWeakPtr());
  training_is_in_progress_ = true;
  // Note that this copies the training data, so it's okay if we add more
  // examples to our copy before this returns.
  // TODO(liberato): Post to a background task runner, and bind |model_cb| to
  // the current one.  Be careful about ownership if we invalidate |trainer_|
  // on this thread.  Be sure to post destruction to that sequence.
  trainer_->Train(task_, *training_data_, std::move(model_cb));
}

void LearningTaskControllerImpl::OnModelTrained(std::unique_ptr<Model> model) {
  DCHECK(training_is_in_progress_);
  training_is_in_progress_ = false;
  model_ = std::move(model);
}

void LearningTaskControllerImpl::SetTrainerForTesting(
    std::unique_ptr<TrainingAlgorithm> trainer) {
  trainer_ = std::move(trainer);
}

}  // namespace learning
}  // namespace media
