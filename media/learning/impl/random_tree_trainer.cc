// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/impl/random_tree_trainer.h"

#include <math.h>

#include "base/bind.h"
#include "base/logging.h"
#include "base/optional.h"

namespace media {
namespace learning {

// static
TrainingAlgorithmCB RandomTreeTrainer::GetTrainingAlgorithmCB(
    const LearningTask& task) {
  return base::BindRepeating(
      [](LearningTask task, TrainingData training_data,
         TrainedModelCB model_cb) {
        std::move(model_cb).Run(RandomTreeTrainer().Train(task, training_data));
      },
      task);
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
  InteriorNode(const LearningTask& task,
               int split_index,
               FeatureValue split_point)
      : split_index_(split_index),
        rt_unknown_value_handling_(task.rt_unknown_value_handling),
        ordering_(task.feature_descriptions[split_index].ordering),
        split_point_(split_point) {}

  // Model
  TargetDistribution PredictDistribution(
      const FeatureVector& features) override {
    // Figure out what feature value we should use for the split.
    FeatureValue f;
    switch (ordering_) {
      case LearningTask::Ordering::kUnordered:
        // Use the nominal value directly.
        f = features[split_index_];
        break;
      case LearningTask::Ordering::kNumeric:
        // Use 0 for "<=" and 1 for ">".
        f = FeatureValue(features[split_index_] > split_point_);
        break;
    }

    auto iter = children_.find(f);

    // If we've never seen this feature value, then average all our branches.
    // This is an attempt to mimic one-hot encoding, where we'll take the zero
    // branch but it depends on the tree structure which of the one-hot values
    // we're choosing.
    if (iter == children_.end()) {
      switch (rt_unknown_value_handling_) {
        case LearningTask::RTUnknownValueHandling::kEmptyDistribution:
          return TargetDistribution();
        case LearningTask::RTUnknownValueHandling::kUseAllSplits:
          return PredictDistributionWithMissingValues(features);
      }
    }

    return iter->second->PredictDistribution(features);
  }

  TargetDistribution PredictDistributionWithMissingValues(
      const FeatureVector& features) {
    TargetDistribution total;
    for (auto& child_pair : children_) {
      TargetDistribution predicted =
          child_pair.second->PredictDistribution(features);
      // TODO(liberato): Normalize?  Weight?
      total += predicted;
    }

    return total;
  }

  // Add |child| has the node for feature value |v|.
  void AddChild(FeatureValue v, std::unique_ptr<Model> child) {
    DCHECK_EQ(children_.count(v), 0u);
    children_.emplace(v, std::move(child));
  }

 private:
  // Feature value that we split on.
  int split_index_ = -1;
  base::flat_map<FeatureValue, std::unique_ptr<Model>> children_;

  // How we handle unknown values.
  LearningTask::RTUnknownValueHandling rt_unknown_value_handling_;

  // How is our feature value ordered?
  LearningTask::Ordering ordering_;

  // For kNumeric features, this is the split point.
  FeatureValue split_point_;
};

struct LeafNode : public Model {
  LeafNode(const TrainingData& training_data) {
    for (WeightedExample example : training_data)
      distribution_ += example;
  }

  // TreeNode
  TargetDistribution PredictDistribution(const FeatureVector&) override {
    return distribution_;
  }

 private:
  TargetDistribution distribution_;
};

RandomTreeTrainer::RandomTreeTrainer(RandomNumberGenerator* rng)
    : HasRandomNumberGenerator(rng) {}

RandomTreeTrainer::~RandomTreeTrainer() = default;

std::unique_ptr<Model> RandomTreeTrainer::Train(
    const LearningTask& task,
    const TrainingData& training_data) {
  if (training_data.empty())
    return std::make_unique<LeafNode>(training_data);

  return Build(task, training_data, FeatureSet());
}

std::unique_ptr<Model> RandomTreeTrainer::Build(
    const LearningTask& task,
    const TrainingData& training_data,
    const FeatureSet& used_set) {
  DCHECK(training_data.weighted_size());

  // TODO(liberato): Does it help if we refuse to split without an info gain?
  Split best_potential_split;

  // Select the feature subset to consider at this leaf.
  FeatureSet feature_candidates;
  for (size_t i = 0; i < training_data.begin()->example()->features.size();
       i++) {
    if (used_set.find(i) != used_set.end())
      continue;
    feature_candidates.insert(i);
  }
  // TODO(liberato): Let our caller override this.
  const size_t features_per_split =
      std::min(static_cast<int>(sqrt(feature_candidates.size())), 3);
  while (feature_candidates.size() > features_per_split) {
    // Remove a random feature.
    size_t which = rng()->Generate(feature_candidates.size());
    auto iter = feature_candidates.begin();
    for (; which; which--, iter++)
      ;
    feature_candidates.erase(iter);
  }

  // Find the best split among the candidates that we have.
  for (int i : feature_candidates) {
    Split potential_split = ConstructSplit(task, training_data, i);
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
  std::unique_ptr<InteriorNode> node = std::make_unique<InteriorNode>(
      task, best_potential_split.split_index, best_potential_split.split_point);

  // Don't let the subtree use this feature if this is nominal split, since
  // there's nothing left to split.  For numeric splits, we might want to split
  // it further.  Note that if there is only one branch for this split, then
  // we returned a leaf anyway.
  FeatureSet new_used_set(used_set);
  if (task.feature_descriptions[best_potential_split.split_index].ordering ==
      LearningTask::Ordering::kUnordered) {
    new_used_set.insert(best_potential_split.split_index);
  }

  for (auto& branch_iter : best_potential_split.branch_infos) {
    node->AddChild(branch_iter.first,
                   Build(task, branch_iter.second.training_data, new_used_set));
  }

  return node;
}

RandomTreeTrainer::Split RandomTreeTrainer::ConstructSplit(
    const LearningTask& task,
    const TrainingData& training_data,
    int index) {
  // We should not be given a training set of size 0, since there's no need to
  // check an empty split.
  DCHECK_GT(training_data.weighted_size(), 0u);

  Split split(index);
  base::Optional<FeatureValue> split_point;

  // For a numeric split, find the split point.  Otherwise, we'll split on every
  // nominal value that this feature has in |training_data|.
  if (task.feature_descriptions[index].ordering ==
      LearningTask::Ordering::kNumeric) {
    split_point = FindNumericSplitPoint(split.split_index, training_data);
    split.split_point = *split_point;
  }

  // Find the split's feature values and construct the training set for each.
  // I think we want to iterate on the underlying vector, and look up the int in
  // the training data directly.
  for (WeightedExample weighted_example : training_data) {
    const TrainingExample* example = weighted_example.example();
    // Get the value of the |index|-th feature for |example|.
    FeatureValue v_i = example->features[split.split_index];

    // Figure out what value this example would use for splitting.  For nominal,
    // it's just |v_i|.  For numeric, it's whether |v_i| is <= the split point
    // or not (0 for <=, 1 for >).
    FeatureValue split_feature;
    if (split_point)
      split_feature = FeatureValue(v_i > *split_point);
    else
      split_feature = v_i;

    // Add |v_i| to the right training set.  Remember that emplace will do
    // nothing if the key already exists.
    auto result = split.branch_infos.emplace(
        split_feature, Split::BranchInfo(training_data.storage()));
    auto iter = result.first;

    Split::BranchInfo& branch_info = iter->second;
    branch_info.training_data.push_back(example);
    branch_info.target_distribution += weighted_example;
  }

  // Compute the nats given that we're at this node.
  split.nats_remaining = 0;
  for (auto& info_iter : split.branch_infos) {
    Split::BranchInfo& branch_info = info_iter.second;

    const double total_counts = branch_info.target_distribution.total_counts();
    // |p_branch| is the probability of following this branch.
    const double p_branch =
        ((double)total_counts) / training_data.weighted_size();
    for (auto& iter : branch_info.target_distribution) {
      double p = iter.second / total_counts;
      // p*log(p) is the expected nats if the answer is |iter|.  We multiply
      // that by the probability of being in this bucket at all.
      split.nats_remaining -= (p * log(p)) * p_branch;
    }
  }

  return split;
}

FeatureValue RandomTreeTrainer::FindNumericSplitPoint(
    size_t index,
    const TrainingData& training_data) {
  // We should not be given a training set of size 0, since there's no need to
  // check an empty split.
  DCHECK_GT(training_data.weighted_size(), 0u);

  // We should either (a) choose the single best split point given all our
  // training data (i.e., choosing between the splits that are equally between
  // adjacent feature values), or (b) choose the best split point by drawing
  // uniformly over the range that contains our feature values.  (a) is
  // appropriate with RandomForest, while (b) is appropriate with ExtraTrees.
  FeatureValue v_min = (*training_data.begin()).example()->features[index];
  FeatureValue v_max = (*training_data.begin()).example()->features[index];
  for (WeightedExample weighted_example : training_data) {
    const TrainingExample* example = weighted_example.example();
    // Get the value of the |index|-th feature for
    FeatureValue v_i = example->features[index];
    if (v_i < v_min)
      v_min = v_i;

    if (v_i > v_max)
      v_max = v_i;
  }

  FeatureValue v_split;
  if (v_max == v_min) {
    // Pick |v_split| to return a trivial split, so that this ends up as a
    // leaf node anyway.
    v_split = v_max;
  } else {
    // Choose a random split point.  Note that we want to end up with two
    // buckets, so we don't have a trivial split.  By picking [v_min, v_max),
    // |v_min| will always be in one bucket and |v_max| will always not be.
    v_split = FeatureValue((rand() % (v_max.value() - v_min.value())) +
                           v_min.value());
  }

  return v_split;
}

}  // namespace learning
}  // namespace media
