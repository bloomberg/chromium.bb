// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/impl/random_tree_trainer.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "media/learning/impl/test_random_number_generator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace learning {

class RandomTreeTest : public testing::TestWithParam<LearningTask::Ordering> {
 public:
  RandomTreeTest()
      : rng_(0),
        trainer_(&rng_),
        ordering_(GetParam()) {}

  // Set up |task_| to have |n| features with the given ordering.
  void SetupFeatures(size_t n) {
    for (size_t i = 0; i < n; i++) {
      LearningTask::ValueDescription desc;
      desc.ordering = ordering_;
      task_.feature_descriptions.push_back(desc);
    }
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;

  TestRandomNumberGenerator rng_;
  RandomTreeTrainer trainer_;
  LearningTask task_;
  // Feature ordering.
  LearningTask::Ordering ordering_;
};

TEST_P(RandomTreeTest, EmptyTrainingDataWorks) {
  TrainingData empty;
  std::unique_ptr<Model> model = trainer_.Train(task_, empty);
  EXPECT_NE(model.get(), nullptr);
  EXPECT_EQ(model->PredictDistribution(FeatureVector()), TargetDistribution());
}

TEST_P(RandomTreeTest, UniformTrainingDataWorks) {
  SetupFeatures(2);
  TrainingExample example({FeatureValue(123), FeatureValue(456)},
                          TargetValue(789));
  TrainingData training_data;
  const size_t n_examples = 10;
  for (size_t i = 0; i < n_examples; i++)
    training_data.push_back(example);
  std::unique_ptr<Model> model = trainer_.Train(task_, training_data);

  // The tree should produce a distribution for one value (our target), which
  // has |n_examples| counts.
  TargetDistribution distribution =
      model->PredictDistribution(example.features);
  EXPECT_EQ(distribution.size(), 1u);
  EXPECT_EQ(distribution[example.target_value], n_examples);
}

TEST_P(RandomTreeTest, UniformTrainingDataWorksWithCallback) {
  SetupFeatures(2);
  TrainingExample example({FeatureValue(123), FeatureValue(456)},
                          TargetValue(789));
  TrainingData training_data;
  const size_t n_examples = 10;
  for (size_t i = 0; i < n_examples; i++)
    training_data.push_back(example);

  // Construct a TrainedModelCB that will store the model locally.
  std::unique_ptr<Model> model;
  TrainedModelCB model_cb = base::BindOnce(
      [](std::unique_ptr<Model>* model_out, std::unique_ptr<Model> model) {
        *model_out = std::move(model);
      },
      &model);

  // Run the trainer.
  RandomTreeTrainer::GetTrainingAlgorithmCB(task_).Run(training_data,
                                                       std::move(model_cb));
  base::RunLoop().RunUntilIdle();

  TargetDistribution distribution =
      model->PredictDistribution(example.features);
  EXPECT_EQ(distribution.size(), 1u);
  EXPECT_EQ(distribution[example.target_value], n_examples);
}

TEST_P(RandomTreeTest, SimpleSeparableTrainingData) {
  SetupFeatures(1);
  TrainingData training_data;
  TrainingExample example_1({FeatureValue(123)}, TargetValue(1));
  TrainingExample example_2({FeatureValue(456)}, TargetValue(2));
  training_data.push_back(example_1);
  training_data.push_back(example_2);
  std::unique_ptr<Model> model = trainer_.Train(task_, training_data);

  // Each value should have a distribution with one target value with one count.
  TargetDistribution distribution =
      model->PredictDistribution(example_1.features);
  EXPECT_NE(model.get(), nullptr);
  EXPECT_EQ(distribution.size(), 1u);
  EXPECT_EQ(distribution[example_1.target_value], 1u);

  distribution = model->PredictDistribution(example_2.features);
  EXPECT_EQ(distribution.size(), 1u);
  EXPECT_EQ(distribution[example_2.target_value], 1u);
}

TEST_P(RandomTreeTest, ComplexSeparableTrainingData) {
  // Building a random tree with numeric splits isn't terribly likely to work,
  // so just skip it.  Entirely randomized splits are just too random.  The
  // RandomForest unittests will test them as part of an ensemble.
  if (ordering_ == LearningTask::Ordering::kNumeric)
    return;

  SetupFeatures(4);
  // Build a four-feature training set that's completely separable, but one
  // needs all four features to do it.
  TrainingData training_data;
  for (int f1 = 0; f1 < 2; f1++) {
    for (int f2 = 0; f2 < 2; f2++) {
      for (int f3 = 0; f3 < 2; f3++) {
        for (int f4 = 0; f4 < 2; f4++) {
          TrainingExample example(
              {FeatureValue(f1), FeatureValue(f2), FeatureValue(f3),
               FeatureValue(f4)},
              TargetValue(f1 * 1 + f2 * 2 + f3 * 4 + f4 * 8));
          // Add two copies of each example.
          training_data.push_back(example);
          training_data.push_back(example);
        }
      }
    }
  }

  std::unique_ptr<Model> model = trainer_.Train(task_, training_data);
  EXPECT_NE(model.get(), nullptr);

  // Each example should have a distribution that selects the right value.
  for (const TrainingExample& example : training_data) {
    TargetDistribution distribution =
        model->PredictDistribution(example.features);
    TargetValue singular_max;
    EXPECT_TRUE(distribution.FindSingularMax(&singular_max));
    EXPECT_EQ(singular_max, example.target_value);
  }
}

TEST_P(RandomTreeTest, UnseparableTrainingData) {
  SetupFeatures(2);
  TrainingData training_data;
  TrainingExample example_1({FeatureValue(123)}, TargetValue(1));
  TrainingExample example_2({FeatureValue(123)}, TargetValue(2));
  training_data.push_back(example_1);
  training_data.push_back(example_2);
  std::unique_ptr<Model> model = trainer_.Train(task_, training_data);
  EXPECT_NE(model.get(), nullptr);

  // Each value should have a distribution with two targets with one count each.
  TargetDistribution distribution =
      model->PredictDistribution(example_1.features);
  EXPECT_EQ(distribution.size(), 2u);
  EXPECT_EQ(distribution[example_1.target_value], 1u);
  EXPECT_EQ(distribution[example_2.target_value], 1u);

  distribution = model->PredictDistribution(example_2.features);
  EXPECT_EQ(distribution.size(), 2u);
  EXPECT_EQ(distribution[example_1.target_value], 1u);
  EXPECT_EQ(distribution[example_2.target_value], 1u);
}

TEST_P(RandomTreeTest, UnknownFeatureValueHandling) {
  // Verify how a previously unseen feature value is handled.
  SetupFeatures(2);
  TrainingData training_data;
  TrainingExample example_1({FeatureValue(123)}, TargetValue(1));
  TrainingExample example_2({FeatureValue(456)}, TargetValue(2));
  training_data.push_back(example_1);
  training_data.push_back(example_2);

  task_.rt_unknown_value_handling =
      LearningTask::RTUnknownValueHandling::kEmptyDistribution;
  std::unique_ptr<Model> model = trainer_.Train(task_, training_data);
  TargetDistribution distribution =
      model->PredictDistribution(FeatureVector({FeatureValue(789)}));
  if (ordering_ == LearningTask::Ordering::kUnordered) {
    // OOV data should return an empty distribution (nominal).
    EXPECT_EQ(distribution.size(), 0u);
  } else {
    // OOV data should end up in the |example_2| bucket, since the feature is
    // numerically higher.
    EXPECT_EQ(distribution.size(), 1u);
    EXPECT_EQ(distribution[example_2.target_value], 1u);
  }

  task_.rt_unknown_value_handling =
      LearningTask::RTUnknownValueHandling::kUseAllSplits;
  model = trainer_.Train(task_, training_data);
  distribution = model->PredictDistribution(FeatureVector({FeatureValue(789)}));
  if (ordering_ == LearningTask::Ordering::kUnordered) {
    // OOV data should return with the sum of all splits.
    EXPECT_EQ(distribution.size(), 2u);
    EXPECT_EQ(distribution[example_1.target_value], 1u);
    EXPECT_EQ(distribution[example_2.target_value], 1u);
  } else {
    // The unknown feature is numerically higher than |example_2|, so we
    // expect it to fall into that bucket.
    EXPECT_EQ(distribution.size(), 1u);
    EXPECT_EQ(distribution[example_2.target_value], 1u);
  }
}

TEST_P(RandomTreeTest, NumericFeaturesSplitMultipleTimes) {
  // Verify that numeric features can be split more than once in the tree.
  // This should also pass for nominal features, though it's less interesting.
  SetupFeatures(2);
  TrainingData training_data;
  const int feature_mult = 10;
  for (size_t i = 0; i < 4; i++) {
    TrainingExample example({FeatureValue(i * feature_mult)}, TargetValue(i));
    training_data.push_back(example);
  }

  task_.rt_unknown_value_handling =
      LearningTask::RTUnknownValueHandling::kEmptyDistribution;
  std::unique_ptr<Model> model = trainer_.Train(task_, training_data);
  for (size_t i = 0; i < 4; i++) {
    // Get a prediction for the |i|-th feature value.
    TargetDistribution distribution = model->PredictDistribution(
        FeatureVector({FeatureValue(i * feature_mult)}));
    // The distribution should have one count that should be correct.  If
    // the feature isn't split four times, then some feature value will have too
    // many or too few counts.
    EXPECT_EQ(distribution.total_counts(), 1u);
    EXPECT_EQ(distribution[TargetValue(i)], 1u);
  }
}

INSTANTIATE_TEST_CASE_P(RandomTreeTest,
                        RandomTreeTest,
                        testing::ValuesIn({LearningTask::Ordering::kUnordered,
                                           LearningTask::Ordering::kNumeric}));

}  // namespace learning
}  // namespace media
