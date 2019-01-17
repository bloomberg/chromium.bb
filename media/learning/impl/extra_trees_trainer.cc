// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/impl/extra_trees_trainer.h"

#include <set>

#include "base/bind.h"
#include "base/logging.h"
#include "media/learning/impl/random_tree_trainer.h"
#include "media/learning/impl/voting_ensemble.h"

namespace media {
namespace learning {

ExtraTreesTrainer::ExtraTreesTrainer() = default;

ExtraTreesTrainer::~ExtraTreesTrainer() = default;

void ExtraTreesTrainer::Train(const LearningTask& task,
                              const TrainingData& training_data,
                              TrainedModelCB model_cb) {
  // Make sure that there is no training in progress.
  DCHECK_EQ(trees_.size(), 0u);
  DCHECK_EQ(converter_.get(), nullptr);

  task_ = task;
  trees_.reserve(task.rf_number_of_trees);

  // RandomTree requires one-hot vectors to properly choose split points the way
  // that ExtraTrees require.
  // TODO(liberato): Modify it not to need this.  It's slow.
  converter_ = std::make_unique<OneHotConverter>(task, training_data);
  converted_training_data_ = converter_->Convert(training_data);

  // Start training.  Send in nullptr to start the process.
  OnRandomTreeModel(std::move(model_cb), nullptr);
}

void ExtraTreesTrainer::OnRandomTreeModel(TrainedModelCB model_cb,
                                          std::unique_ptr<Model> model) {
  // Allow a null Model to make it easy to start training.
  if (model)
    trees_.push_back(std::move(model));

  // If this is the last tree, then return the finished model.
  if (trees_.size() == task_.rf_number_of_trees) {
    std::move(model_cb).Run(std::make_unique<ConvertingModel>(
        std::move(converter_),
        std::make_unique<VotingEnsemble>(std::move(trees_))));
    return;
  }

  // Train the next tree.
  auto cb = base::BindOnce(&ExtraTreesTrainer::OnRandomTreeModel, AsWeakPtr(),
                           std::move(model_cb));
  RandomTreeTrainer tree_trainer(rng());
  tree_trainer.Train(converter_->converted_task(), converted_training_data_,
                     std::move(cb));
}

}  // namespace learning
}  // namespace media
