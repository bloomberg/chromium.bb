// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/impl/random_tree_trainer.h"

#include <math.h>

#include "base/bind.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/threading/sequenced_task_runner_handle.h"

namespace media {
namespace learning {

RandomTreeTrainer::Split::Split() = default;

RandomTreeTrainer::Split::Split(int index) : split_index(index) {}

RandomTreeTrainer::Split::Split(Split&& rhs) = default;

RandomTreeTrainer::Split::~Split() = default;

RandomTreeTrainer::Split& RandomTreeTrainer::Split::operator=(Split&& rhs) =
    default;

RandomTreeTrainer::Split::BranchInfo::BranchInfo() = default;

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
  LeafNode(const TrainingData& training_data,
           const std::vector<size_t> training_idx,
           LearningTask::Ordering ordering) {
    for (size_t idx : training_idx)
      distribution_ += training_data[idx];

    // Note that we don't treat numeric targets any differently.  We want to
    // weight the leaf by the number of examples, so replacing it with an
    // average would just introduce rounding errors.  One might as well take the
    // average of the final distribution.
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

RandomTreeTrainer::~RandomTreeTrainer() {}

void RandomTreeTrainer::Train(const LearningTask& task,
                              const TrainingData& training_data,
                              TrainedModelCB model_cb) {
  // Start with all the training data.
  std::vector<size_t> training_idx;
  training_idx.reserve(training_data.size());
  for (size_t idx = 0; idx < training_data.size(); idx++)
    training_idx.push_back(idx);

  // It's a little odd that we don't post training.  Perhaps we should.
  auto model = Train(task, training_data, training_idx);
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(model_cb), std::move(model)));
}

std::unique_ptr<Model> RandomTreeTrainer::Train(
    const LearningTask& task,
    const TrainingData& training_data,
    const std::vector<size_t>& training_idx) {
  if (training_data.empty()) {
    return std::make_unique<LeafNode>(training_data, std::vector<size_t>(),
                                      LearningTask::Ordering::kUnordered);
  }

  DCHECK_EQ(task.feature_descriptions.size(), training_data[0].features.size());

  // Start with all features unused.
  FeatureSet unused_set;
  for (size_t idx = 0; idx < task.feature_descriptions.size(); idx++)
    unused_set.insert(idx);

  return Build(task, training_data, training_idx, unused_set);
}

std::unique_ptr<Model> RandomTreeTrainer::Build(
    const LearningTask& task,
    const TrainingData& training_data,
    const std::vector<size_t>& training_idx,
    const FeatureSet& unused_set) {
  DCHECK_GT(training_idx.size(), 0u);

  // TODO: enforce a minimum number of samples.  ExtraTrees uses 2 for
  // classification, and 5 for regression.

  // Remove any constant attributes in |training_data| from |unused_set|.  Also
  // check if our training data has a constant target value.  For both features
  // and the target value, if the Optional has a value then it's the singular
  // value that we've found so far.  If we find a second one, then we'll clear
  // the Optional.
  base::Optional<TargetValue> target_value(
      training_data[training_idx[0]].target_value);
  std::vector<base::Optional<FeatureValue>> feature_values;
  feature_values.resize(training_data[0].features.size());
  for (size_t feature_idx : unused_set) {
    feature_values[feature_idx] =
        training_data[training_idx[0]].features[feature_idx];
  }
  for (size_t idx : training_idx) {
    const LabelledExample& example = training_data[idx];
    // Record this target value to see if there is more than one.  We skip the
    // insertion if we've already determined that it's not constant.
    if (target_value && target_value != example.target_value)
      target_value.reset();

    // For all features in |unused_set|, see if it's a constant in our subset of
    // the training data.
    for (size_t feature_idx : unused_set) {
      auto& value = feature_values[feature_idx];
      if (value && *value != example.features[feature_idx])
        value.reset();
    }
  }

  // Is the output constant in |training_data|?  If so, then generate a leaf.
  // If we're not normalizing leaves, then this matters since this training data
  // might be split across multiple leaves.
  if (target_value) {
    return std::make_unique<LeafNode>(training_data, training_idx,
                                      task.target_description.ordering);
  }

  // Remove any constant features from the unused set, so that we don't try to
  // split on them.  It would work, but it would be trivially useless.  We also
  // don't want to use one of our potential splits on it.
  FeatureSet new_unused_set = unused_set;
  for (size_t feature_idx : unused_set) {
    auto& value = feature_values[feature_idx];
    if (value)
      new_unused_set.erase(feature_idx);
  }

  // Select the feature subset to consider at this leaf.
  FeatureSet feature_candidates = new_unused_set;
  // TODO(liberato): Let our caller override this.
  const size_t features_per_split =
      std::max(static_cast<int>(sqrt(feature_candidates.size())), 3);
  // Note that it's okay if there are fewer features left; we'll select all of
  // them instead.
  while (feature_candidates.size() > features_per_split) {
    // Remove a random feature.
    size_t which = rng()->Generate(feature_candidates.size());
    auto iter = feature_candidates.begin();
    for (; which; which--, iter++)
      ;
    feature_candidates.erase(iter);
  }

  // TODO(liberato): Does it help if we refuse to split without an info gain?
  Split best_potential_split;

  // Find the best split among the candidates that we have.
  for (int i : feature_candidates) {
    Split potential_split =
        ConstructSplit(task, training_data, training_idx, i);
    if (potential_split.nats_remaining < best_potential_split.nats_remaining) {
      best_potential_split = std::move(potential_split);
    }
  }

  // Note that we can have a split with no index (i.e., no features left, or no
  // feature was an improvement in nats), or with a single index (had features,
  // but all had the same value).  Either way, we should end up with a leaf.
  if (best_potential_split.branch_infos.size() < 2) {
    // Stop when there is no more tree.
    return std::make_unique<LeafNode>(training_data, training_idx,
                                      task.target_description.ordering);
  }

  // Build an interior node
  std::unique_ptr<InteriorNode> node = std::make_unique<InteriorNode>(
      task, best_potential_split.split_index, best_potential_split.split_point);

  // Don't let the subtree use this feature if this is nominal split, since
  // there's nothing left to split.  For numeric splits, we might want to split
  // it further.  Note that if there is only one branch for this split, then
  // we returned a leaf anyway.
  if (task.feature_descriptions[best_potential_split.split_index].ordering ==
      LearningTask::Ordering::kUnordered) {
    DCHECK(new_unused_set.find(best_potential_split.split_index) !=
           new_unused_set.end());
    new_unused_set.erase(best_potential_split.split_index);
  }

  for (auto& branch_iter : best_potential_split.branch_infos) {
    node->AddChild(branch_iter.first,
                   Build(task, training_data, branch_iter.second.training_idx,
                         new_unused_set));
  }

  return node;
}

RandomTreeTrainer::Split RandomTreeTrainer::ConstructSplit(
    const LearningTask& task,
    const TrainingData& training_data,
    const std::vector<size_t>& training_idx,
    int split_index) {
  // We should not be given a training set of size 0, since there's no need to
  // check an empty split.
  DCHECK_GT(training_idx.size(), 0u);

  Split split(split_index);
  base::Optional<FeatureValue> split_point;

  // TODO(liberato): Consider removing nominal feature support and RF.  That
  // would make this code somewhat simpler.

  // For a numeric split, find the split point.  Otherwise, we'll split on every
  // nominal value that this feature has in |training_data|.
  if (task.feature_descriptions[split_index].ordering ==
      LearningTask::Ordering::kNumeric) {
    split_point =
        FindNumericSplitPoint(split.split_index, training_data, training_idx);
    split.split_point = *split_point;
  }

  // Find the split's feature values and construct the training set for each.
  // I think we want to iterate on the underlying vector, and look up the int in
  // the training data directly.
  double total_weight = 0.;
  for (size_t idx : training_idx) {
    const LabelledExample& example = training_data[idx];
    total_weight += example.weight;

    // Get the value of the |index|-th feature for |example|.
    FeatureValue v_i = example.features[split.split_index];

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
    auto result =
        split.branch_infos.emplace(split_feature, Split::BranchInfo());
    auto iter = result.first;

    Split::BranchInfo& branch_info = iter->second;
    branch_info.training_idx.push_back(idx);
    branch_info.target_distribution += example;
  }

  // Figure out how good / bad this split is.
  switch (task.target_description.ordering) {
    case LearningTask::Ordering::kUnordered:
      ComputeNominalSplitScore(&split, total_weight);
      break;
    case LearningTask::Ordering::kNumeric:
      ComputeNumericSplitScore(&split, total_weight);
      break;
  }

  return split;
}

void RandomTreeTrainer::ComputeNominalSplitScore(Split* split,
                                                 double total_weight) {
  // Compute the nats given that we're at this node.
  split->nats_remaining = 0;
  for (auto& info_iter : split->branch_infos) {
    Split::BranchInfo& branch_info = info_iter.second;

    const double total_counts = branch_info.target_distribution.total_counts();
    // |p_branch| is the probability of following this branch.
    const double p_branch = total_counts / total_weight;
    for (auto& iter : branch_info.target_distribution) {
      double p = iter.second / total_counts;
      // p*log(p) is the expected nats if the answer is |iter|.  We multiply
      // that by the probability of being in this bucket at all.
      split->nats_remaining -= (p * log(p)) * p_branch;
    }
  }
}

void RandomTreeTrainer::ComputeNumericSplitScore(Split* split,
                                                 double total_weight) {
  // Compute the nats given that we're at this node.
  split->nats_remaining = 0;
  for (auto& info_iter : split->branch_infos) {
    Split::BranchInfo& branch_info = info_iter.second;

    const double total_counts = branch_info.target_distribution.total_counts();
    // |p_branch| is the probability of following this branch.
    const double p_branch = total_counts / total_weight;

    // Compute the average at this node.  Note that we have no idea if the leaf
    // node would actually use an average, but really it should match.  It would
    // be really nice if we could compute the value (or TargetDistribution) as
    // part of computing the split, and have somebody just hand that target
    // distribution to the leaf if it ends up as one.
    double average = branch_info.target_distribution.Average();

    for (auto& iter : branch_info.target_distribution) {
      // Compute the squared error for all |iter.second| counts that each have a
      // value of |iter.first|, when this leaf approximates them as |average|.
      double sq_err = (iter.first.value() - average) *
                      (iter.first.value() - average) * iter.second;
      split->nats_remaining += sq_err * p_branch;
    }
  }
}

FeatureValue RandomTreeTrainer::FindNumericSplitPoint(
    size_t split_index,
    const TrainingData& training_data,
    const std::vector<size_t>& training_idx) {
  // We should not be given a training set of size 0, since there's no need to
  // check an empty split.
  DCHECK_GT(training_idx.size(), 0u);

  // We should either (a) choose the single best split point given all our
  // training data (i.e., choosing between the splits that are equally between
  // adjacent feature values), or (b) choose the best split point by drawing
  // uniformly over the range that contains our feature values.  (a) is
  // appropriate with RandomForest, while (b) is appropriate with ExtraTrees.
  FeatureValue v_min = training_data[training_idx[0]].features[split_index];
  FeatureValue v_max = training_data[training_idx[0]].features[split_index];
  for (size_t idx : training_idx) {
    const LabelledExample& example = training_data[idx];
    // Get the value of the |split_index|-th feature for
    FeatureValue v_i = example.features[split_index];
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
    v_split = FeatureValue(
        rng()->GenerateDouble(v_max.value() - v_min.value()) + v_min.value());
  }

  return v_split;
}

}  // namespace learning
}  // namespace media
