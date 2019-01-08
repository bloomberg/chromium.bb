// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/impl/random_forest_trainer.h"

#include <set>

#include "base/logging.h"
#include "media/learning/impl/random_tree_trainer.h"
#include "media/learning/impl/voting_ensemble.h"

namespace media {
namespace learning {

RandomForestTrainer::TrainingResult::TrainingResult() = default;

RandomForestTrainer::TrainingResult::~TrainingResult() = default;

RandomForestTrainer::RandomForestTrainer() = default;

RandomForestTrainer::~RandomForestTrainer() = default;

std::unique_ptr<RandomForestTrainer::TrainingResult> RandomForestTrainer::Train(
    const LearningTask& task,
    const TrainingData& training_data) {
  int n_trees = task.rf_number_of_trees;

  RandomTreeTrainer tree_trainer(rng());
  std::vector<std::unique_ptr<Model>> trees;
  trees.reserve(n_trees);

  // [example index] = sum of all oob predictions
  std::map<size_t, TargetDistribution> oob_distributions;

  // We don't support weighted training data, since bagging with weights is
  // very hard to do right without spending time that depends on the value of
  // the weights.  Since ExactTrees don't need bagging, we skip it.
  if (!training_data.is_unweighted())
    return std::make_unique<TrainingResult>();

  const size_t n_examples = training_data.size();
  for (int i = 0; i < n_trees; i++) {
    // Collect a bagged training set and oob data for it.
    std::vector<size_t> bagged_idx;
    bagged_idx.reserve(training_data.size());

    std::set<size_t> bagged_set;
    for (size_t e = 0; e < n_examples; e++) {
      size_t idx = rng()->Generate(n_examples);
      bagged_idx.push_back(idx);
      bagged_set.insert(idx);
    }

    // Train the tree.
    std::unique_ptr<Model> tree =
        tree_trainer.Train(task, training_data, bagged_idx);

    // Compute OOB distribution.
    for (size_t e = 0; e < n_examples; e++) {
      if (bagged_set.find(e) != bagged_set.end())
        continue;

      const LabelledExample& example = training_data[e];

      TargetDistribution predicted =
          tree->PredictDistribution(example.features);

      // Add the predicted distribution to this example's total distribution.
      // Remember that the distribution is not normalized, so the counts will
      // scale with the number of examples.
      // TODO(liberato): Should it be normalized before being combined?
      TargetDistribution& our_oob_dist = oob_distributions[e];
      our_oob_dist += predicted;
    }

    trees.push_back(std::move(tree));
  }

  std::unique_ptr<VotingEnsemble> forest =
      std::make_unique<VotingEnsemble>(std::move(trees));

  // Compute OOB accuracy.
  int num_correct = 0;
  for (auto& oob_pair : oob_distributions) {
    const LabelledExample& example = training_data[oob_pair.first];
    const TargetDistribution& distribution = oob_pair.second;

    // If there are no guesses, or if it's a tie, then count it as wrong.
    TargetValue max_value;
    if (distribution.FindSingularMax(&max_value) &&
        max_value == example.target_value) {
      num_correct++;
    }
  }

  std::unique_ptr<TrainingResult> result = std::make_unique<TrainingResult>();
  result->model = std::move(forest);
  result->oob_correct = num_correct;
  result->oob_total = oob_distributions.size();

  return result;
}

}  // namespace learning
}  // namespace media
