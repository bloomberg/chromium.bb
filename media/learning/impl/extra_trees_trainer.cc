// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/impl/extra_trees_trainer.h"

#include <set>

#include "base/logging.h"
#include "media/learning/impl/one_hot.h"
#include "media/learning/impl/random_tree_trainer.h"
#include "media/learning/impl/voting_ensemble.h"

namespace media {
namespace learning {

ExtraTreesTrainer::ExtraTreesTrainer() = default;

ExtraTreesTrainer::~ExtraTreesTrainer() = default;

std::unique_ptr<Model> ExtraTreesTrainer::Train(
    const LearningTask& task,
    const TrainingData& training_data) {
  int n_trees = task.rf_number_of_trees;

  RandomTreeTrainer tree_trainer(rng());
  std::vector<std::unique_ptr<Model>> trees;
  trees.reserve(n_trees);

  // RandomTree requires one-hot vectors to properly choose split points the way
  // that ExtraTrees require.
  std::unique_ptr<OneHotConverter> converter =
      std::make_unique<OneHotConverter>(task, training_data);
  TrainingData converted_training_data = converter->Convert(training_data);

  for (int i = 0; i < n_trees; i++) {
    // Train the tree.
    std::unique_ptr<Model> tree = tree_trainer.Train(
        converter->converted_task(), converted_training_data);

    trees.push_back(std::move(tree));
  }

  return std::make_unique<ConvertingModel>(
      std::move(converter), std::make_unique<VotingEnsemble>(std::move(trees)));
}

}  // namespace learning
}  // namespace media
