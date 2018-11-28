// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/impl/random_tree_trainer.h"

#include <math.h>

#include "base/bind.h"
#include "base/logging.h"

namespace media {
namespace learning {

// static
TrainingAlgorithmCB RandomTreeTrainer::GetTrainingAlgorithmCB() {
  return base::BindRepeating(
      [](TrainingData training_data, TrainedModelCB model_cb) {
        std::move(model_cb).Run(RandomTreeTrainer().Train(training_data));
      });
}

RandomTreeTrainer::Split::Split() = default;

RandomTreeTrainer::Split::Split(int index) : split_index(index) {}

RandomTreeTrainer::Split::Split(Split&& rhs) = default;

RandomTreeTrainer::Split::~Split() = default;

RandomTreeTrainer::Split& RandomTreeTrainer::Split::operator=(Split&& rhs) =
    default;

RandomTreeTrainer::Split::BranchInfo::BranchInfo(
    scoped_refptr<TrainingDataStorage> storage)
    : training_data(std::move(storage)) {}

RandomTreeTrainer::Split::BranchInfo::BranchInfo(BranchInfo&& rhs) = default;

RandomTreeTrainer::Split::BranchInfo::~BranchInfo() = default;

struct InteriorNode : public Model {
  InteriorNode(int split_index) : split_index_(split_index) {}

  // Model
  Model::TargetDistribution PredictDistribution(
      const FeatureVector& features) override {
    auto iter = children_.find(features[split_index_]);
    // If we've never seen this feature value, then make no prediction.
    if (iter == children_.end())
      return TargetDistribution();

    return iter->second->PredictDistribution(features);
  }

  // Add |child| has the node for feature value |v|.
  void AddChild(FeatureValue v, std::unique_ptr<Model> child) {
    DCHECK_EQ(children_.count(v), 0u);
    children_.emplace(v, std::move(child));
  }

 private:
  // Feature value that we split on.
  int split_index_ = -1;
  std::map<FeatureValue, std::unique_ptr<Model>> children_;
};

struct LeafNode : public Model {
  LeafNode(const TrainingData& training_data) {
    for (const TrainingExample* example : training_data)
      distribution_[example->target_value]++;
  }

  // TreeNode
  Model::TargetDistribution PredictDistribution(const FeatureVector&) override {
    return distribution_;
  }

 private:
  Model::TargetDistribution distribution_;
};

RandomTreeTrainer::RandomTreeTrainer() = default;

RandomTreeTrainer::~RandomTreeTrainer() = default;

std::unique_ptr<Model> RandomTreeTrainer::Train(
    const TrainingData& training_data) {
  if (training_data.empty())
    return std::make_unique<InteriorNode>(-1);

  return Build(training_data, FeatureSet());
}

std::unique_ptr<Model> RandomTreeTrainer::Build(
    const TrainingData& training_data,
    const FeatureSet& used_set) {
  DCHECK(training_data.size());

  // TODO(liberato): Does it help if we refuse to split without an info gain?
  Split best_potential_split;

  // Select the feature subset to consider at this leaf.
  // TODO(liberato): subset.
  FeatureSet feature_candidates;
  for (size_t i = 0; i < training_data[0]->features.size(); i++) {
    if (used_set.find(i) != used_set.end())
      continue;
    feature_candidates.insert(i);
  }

  // Find the best split among the candidates that we have.
  for (int i : feature_candidates) {
    Split potential_split = ConstructSplit(training_data, i);
    if (potential_split.nats_remaining < best_potential_split.nats_remaining) {
      best_potential_split = std::move(potential_split);
    }
  }

  // Note that we can have a split with no index (i.e., no features left, or no
  // feature was an improvement in nats), or with a single index (had features,
  // but all had the same value).  Either way, we should end up with a leaf.
  if (best_potential_split.branch_infos.size() < 2) {
    // Stop when there is no more tree.
    return std::make_unique<LeafNode>(training_data);
  }

  // Build an interior node
  std::unique_ptr<InteriorNode> node =
      std::make_unique<InteriorNode>(best_potential_split.split_index);

  // Don't let the subtree use this feature.
  FeatureSet new_used_set(used_set);
  new_used_set.insert(best_potential_split.split_index);

  for (auto& branch_iter : best_potential_split.branch_infos) {
    node->AddChild(branch_iter.first,
                   Build(branch_iter.second.training_data, new_used_set));
  }

  return node;
}

RandomTreeTrainer::Split RandomTreeTrainer::ConstructSplit(
    const TrainingData& training_data,
    int index) {
  // We should not be given a training set of size 0, since there's no need to
  // check an empty split.
  DCHECK_GT(training_data.size(), 0u);

  Split split(index);

  // Find the split's feature values and construct the training set for each.
  // I think we want to iterate on the underlying vector, and look up the int in
  // the training data directly.
  for (const TrainingExample* example : training_data) {
    // Get the value of the |index|-th feature for
    FeatureValue v_i = example->features[split.split_index];

    // Add |v_i| to the right training set.  Remember that emplace will do
    // nothing if the key already exists.
    auto result = split.branch_infos.emplace(
        v_i, Split::BranchInfo(training_data.storage()));
    auto iter = result.first;

    Split::BranchInfo& branch_info = iter->second;
    branch_info.training_data.push_back(example);
    branch_info.class_counts[example->target_value]++;
  }

  // Compute the nats given that we're at this node.
  split.nats_remaining = 0;
  for (auto& info_iter : split.branch_infos) {
    Split::BranchInfo& branch_info = info_iter.second;

    const int total_counts = branch_info.training_data.size();
    for (auto& iter : branch_info.class_counts) {
      double p = ((double)iter.second) / total_counts;
      split.nats_remaining -= p * log(p);
    }
  }

  return split;
}

}  // namespace learning
}  // namespace media
