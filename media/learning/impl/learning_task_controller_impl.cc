// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/impl/learning_task_controller_impl.h"

#include <memory>
#include <utility>
#include <vector>

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
      reporter_(std::move(reporter)) {
  // TODO(liberato): Make this compositional.  FeatureSubsetTaskController?
  if (task_.feature_subset_size)
    DoFeatureSubsetSelection();

  // Now that we have the updated task, create a helper for it.
  helper_ = std::make_unique<LearningTaskControllerHelper>(
      task,
      base::BindRepeating(&LearningTaskControllerImpl::AddFinishedExample,
                          AsWeakPtr()),
      std::move(feature_provider));

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
  // Copy just the subset we care about.
  FeatureVector new_features;
  if (task_.feature_subset_size) {
    for (auto& iter : feature_indices_)
      new_features.push_back(features[iter]);
  } else {
    // Use them all.
    new_features = features;
  }

  helper_->BeginObservation(id, new_features);
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
    TargetHistogram predicted = model_->PredictDistribution(example.features);

    TargetHistogram observed;
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

void LearningTaskControllerImpl::DoFeatureSubsetSelection() {
  // Choose a random feature, and trim the descriptions to match.
  std::vector<size_t> features;
  for (size_t i = 0; i < task_.feature_descriptions.size(); i++)
    features.push_back(i);

  for (int i = 0; i < *task_.feature_subset_size; i++) {
    // Pick an element from |i| to the end of the list, inclusive.
    // TODO(liberato): For tests, this will happen before any rng is provided
    // by the test; we'll use an actual rng.
    int r = rng()->Generate(features.size() - i) + i;
    // Swap them.
    std::swap(features[i], features[r]);
  }

  // Construct the feature subset from the first few elements.  Also adjust the
  // task's descriptions to match.  We do this in two steps so that the
  // descriptions are added via iterating over |feature_indices_|, so that the
  // enumeration order is the same as when we adjust the feature values of
  // incoming examples.  In both cases, we iterate over |feature_indicies_|,
  // which might (will) re-order them with respect to |features|.
  for (int i = 0; i < *task_.feature_subset_size; i++)
    feature_indices_.insert(features[i]);

  std::vector<LearningTask::ValueDescription> adjusted_descriptions;
  for (auto& iter : feature_indices_)
    adjusted_descriptions.push_back(task_.feature_descriptions[iter]);

  task_.feature_descriptions = adjusted_descriptions;

  reporter_->SetFeatureSubset(feature_indices_);
}

}  // namespace learning
}  // namespace media
