// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/impl/random_forest_trainer.h"

#include <set>

#include "base/logging.h"
#include "media/learning/impl/random_forest.h"
#include "media/learning/impl/random_tree_trainer.h"

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

  // [example] = sum of all oob predictions
  std::map<const TrainingExample*, TargetDistribution> oob_distributions;

  const int n_examples = training_data.size();
  for (int i = 0; i < n_trees; i++) {
    // Collect a bagged training set.
    TrainingData bagged_data(training_data.storage());

    std::set<const TrainingExample*> bagged_set;
    for (int e = 0; e < n_examples; e++) {
      const TrainingExample* example =
          training_data[rng()->Generate(n_examples)];
      bagged_data.push_back(example);
      bagged_set.insert(example);
    }

    // Train the tree.
    std::unique_ptr<Model> tree = tree_trainer.Train(task, bagged_data);

    // Compute OOB distribution.
    int n_oob = 0;
    for (const TrainingExample* example : training_data) {
      if (bagged_set.find(example) != bagged_set.end())
        continue;

      n_oob++;
      TargetDistribution predicted =
          tree->PredictDistribution(example->features);

      TargetDistribution& our_oob_dist = oob_distributions[example];
      our_oob_dist += predicted;
    }

    trees.push_back(std::move(tree));
  }

  std::unique_ptr<RandomForest> forest =
      std::make_unique<RandomForest>(std::move(trees));

  // Compute OOB accuracy.
  int num_correct = 0;
  for (auto& oob_pair : oob_distributions) {
    const TrainingExample* example = oob_pair.first;
    const TargetDistribution& distribution = oob_pair.second;

    // If there are no guesses, or if it's a tie, then count it as wrong.
    TargetValue max_value;
    if (distribution.FindSingularMax(&max_value) &&
        max_value == example->target_value) {
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
