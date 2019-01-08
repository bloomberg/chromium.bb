// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/impl/random_forest_trainer.h"

#include "base/memory/ref_counted.h"
#include "media/learning/impl/fisher_iris_dataset.h"
#include "media/learning/impl/test_random_number_generator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace learning {

class RandomForestTest : public testing::TestWithParam<LearningTask::Ordering> {
 public:
  RandomForestTest()
      : rng_(0),
        ordering_(GetParam()) {
    trainer_.SetRandomNumberGeneratorForTesting(&rng_);
  }

  // Set up |task_| to have |n| features with the given ordering.
  void SetupFeatures(size_t n) {
    for (size_t i = 0; i < n; i++) {
      LearningTask::ValueDescription desc;
      desc.ordering = ordering_;
      task_.feature_descriptions.push_back(desc);
    }
  }

  TestRandomNumberGenerator rng_;
  RandomForestTrainer trainer_;
  LearningTask task_;
  // Feature ordering.
  LearningTask::Ordering ordering_;
};

TEST_P(RandomForestTest, EmptyTrainingDataWorks) {
  TrainingData empty;
  auto result = trainer_.Train(task_, empty);
  EXPECT_NE(result->model.get(), nullptr);
  EXPECT_EQ(result->model->PredictDistribution(FeatureVector()),
            TargetDistribution());
}

TEST_P(RandomForestTest, UniformTrainingDataWorks) {
  SetupFeatures(2);
  LabelledExample example({FeatureValue(123), FeatureValue(456)},
                          TargetValue(789));
  const int n_examples = 10;

  // We need distinct pointers.
  TrainingData training_data;
  for (int i = 0; i < n_examples; i++)
    training_data.push_back(example);

  auto result = trainer_.Train(task_, training_data);

  // The tree should produce a distribution for one value (our target), which
  // has |n_examples| counts.
  TargetDistribution distribution =
      result->model->PredictDistribution(example.features);
  EXPECT_EQ(distribution.size(), 1u);
}

TEST_P(RandomForestTest, SimpleSeparableTrainingData) {
  SetupFeatures(1);
  // TODO: oob estimates aren't so good if a target only shows up once.  any
  // tree that trains on it won't be used to predict it during oob accuracy,
  // and the remaining trees will get it wrong.
  LabelledExample example_1({FeatureValue(123)}, TargetValue(1));
  LabelledExample example_2({FeatureValue(456)}, TargetValue(2));
  TrainingData training_data;
  training_data.push_back(example_1);
  training_data.push_back(example_2);
  auto result = trainer_.Train(task_, training_data);

  // Each value should have a distribution with the correct target value.
  TargetDistribution distribution =
      result->model->PredictDistribution(example_1.features);
  EXPECT_NE(result->model.get(), nullptr);
  TargetValue max_1;
  EXPECT_TRUE(distribution.FindSingularMax(&max_1));
  EXPECT_EQ(max_1, example_1.target_value);

  distribution = result->model->PredictDistribution(example_2.features);
  TargetValue max_2;
  EXPECT_TRUE(distribution.FindSingularMax(&max_2));
  EXPECT_EQ(max_2, example_2.target_value);
}

TEST_P(RandomForestTest, ComplexSeparableTrainingData) {
  SetupFeatures(4);
  // Build a four-feature training set that's completely separable, but one
  // needs all four features to do it.
  TrainingData training_data;
  for (int f1 = 0; f1 < 2; f1++) {
    for (int f2 = 0; f2 < 2; f2++) {
      for (int f3 = 0; f3 < 2; f3++) {
        for (int f4 = 0; f4 < 2; f4++) {
          LabelledExample example(
              {FeatureValue(f1), FeatureValue(f2), FeatureValue(f3),
               FeatureValue(f4)},
              TargetValue(f1 * 1 + f2 * 2 + f3 * 4 + f4 * 8));
          // Add two distinct copies of each example.
          // i guess we don't need to, but oob estimation won't work.
          training_data.push_back(example);
          training_data.push_back(example);
        }
      }
    }
  }

  auto result = trainer_.Train(task_, training_data);
  EXPECT_NE(result->model.get(), nullptr);

  // Each example should have a distribution in which it is the max.
  for (const LabelledExample& example : training_data) {
    TargetDistribution distribution =
        result->model->PredictDistribution(example.features);
    TargetValue max_value;
    EXPECT_TRUE(distribution.FindSingularMax(&max_value));
    EXPECT_EQ(max_value, example.target_value);
  }
}

TEST_P(RandomForestTest, UnseparableTrainingData) {
  SetupFeatures(1);
  LabelledExample example_1({FeatureValue(123)}, TargetValue(1));
  LabelledExample example_2({FeatureValue(123)}, TargetValue(2));
  TrainingData training_data;
  training_data.push_back(example_1);
  training_data.push_back(example_2);
  auto result = trainer_.Train(task_, training_data);
  EXPECT_NE(result->model.get(), nullptr);

  // Each value should have a distribution with two targets.
  TargetDistribution distribution =
      result->model->PredictDistribution(example_1.features);
  EXPECT_EQ(distribution.size(), 2u);

  distribution = result->model->PredictDistribution(example_2.features);
  EXPECT_EQ(distribution.size(), 2u);
}

TEST_P(RandomForestTest, FisherIrisDataset) {
  SetupFeatures(4);
  FisherIrisDataset iris;
  TrainingData training_data = iris.GetTrainingData();
  auto result = trainer_.Train(task_, training_data);

  // Require at least 75% oob data.  Should probably be ~100%.
  EXPECT_GT(result->oob_total, training_data.total_weight() * 0.75);

  // Require at least 85% oob accuracy.  We actually get about 88% (kUnordered)
  // or 95% (kOrdered).
  double oob_accuracy = ((double)result->oob_correct) / result->oob_total;
  EXPECT_GT(oob_accuracy, 0.85);

  // Verify predictions on the training set, just for sanity.
  size_t num_correct = 0;
  for (const LabelledExample& example : training_data) {
    TargetDistribution distribution =
        result->model->PredictDistribution(example.features);
    TargetValue predicted_value;
    if (distribution.FindSingularMax(&predicted_value) &&
        predicted_value == example.target_value) {
      num_correct += example.weight;
    }
  }

  // Expect very high accuracy.  We should get ~100%.
  // Currently, we seem to get about ~95%.  If we switch to kEmptyDistribution
  // in the learning task, then it goes back to 1 for kUnordered features.  It's
  // 1 for kOrdered.
  double train_accuracy = ((double)num_correct) / training_data.total_weight();
  EXPECT_GT(train_accuracy, 0.95);
}

TEST_P(RandomForestTest, WeightedTrainingSetIsUnsupported) {
  LabelledExample example_1({FeatureValue(123)}, TargetValue(1));
  LabelledExample example_2({FeatureValue(123)}, TargetValue(2));
  const size_t weight = 100;
  TrainingData training_data;
  example_1.weight = weight;
  training_data.push_back(example_1);
  example_2.weight = weight;
  training_data.push_back(example_2);

  // Create a weighed set with |weight| for each example's weight.
  EXPECT_FALSE(training_data.is_unweighted());
  auto weighted_result = trainer_.Train(task_, training_data);
  EXPECT_EQ(weighted_result->model.get(), nullptr);
}

INSTANTIATE_TEST_CASE_P(RandomForestTest,
                        RandomForestTest,
                        testing::ValuesIn({LearningTask::Ordering::kUnordered,
                                           LearningTask::Ordering::kNumeric}));

}  // namespace learning
}  // namespace media
