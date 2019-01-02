// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/impl/extra_trees_trainer.h"

#include "base/memory/ref_counted.h"
#include "media/learning/impl/fisher_iris_dataset.h"
#include "media/learning/impl/test_random_number_generator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace learning {

class ExtraTreesTest : public testing::TestWithParam<LearningTask::Ordering> {
 public:
  ExtraTreesTest() : rng_(0), ordering_(GetParam()) {
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
  ExtraTreesTrainer trainer_;
  LearningTask task_;
  // Feature ordering.
  LearningTask::Ordering ordering_;
};

TEST_P(ExtraTreesTest, EmptyTrainingDataWorks) {
  TrainingData empty;
  auto model = trainer_.Train(task_, empty);
  EXPECT_NE(model.get(), nullptr);
  EXPECT_EQ(model->PredictDistribution(FeatureVector()), TargetDistribution());
}

TEST_P(ExtraTreesTest, FisherIrisDataset) {
  SetupFeatures(4);
  FisherIrisDataset iris;
  TrainingData training_data = iris.GetTrainingData();
  auto model = trainer_.Train(task_, training_data);

  // Verify predictions on the training set, just for sanity.
  size_t num_correct = 0;
  for (const TrainingExample& example : training_data) {
    TargetDistribution distribution =
        model->PredictDistribution(example.features);
    TargetValue predicted_value;
    if (distribution.FindSingularMax(&predicted_value) &&
        predicted_value == example.target_value) {
      num_correct += example.weight;
    }
  }

  // Expect very high accuracy.  We should get ~100%.
  // We get about 96% for one-hot features, and 100% for numeric.  Since the
  // data really is numeric, that seems reasonable.
  double train_accuracy = ((double)num_correct) / training_data.total_weight();
  EXPECT_GT(train_accuracy, 0.95);
}

TEST_P(ExtraTreesTest, WeightedTrainingSetIsSupported) {
  // Create a training set with unseparable data, but give one of them a large
  // weight.  See if that one wins.
  SetupFeatures(1);
  TrainingExample example_1({FeatureValue(123)}, TargetValue(1));
  TrainingExample example_2({FeatureValue(123)}, TargetValue(2));
  const size_t weight = 100;
  TrainingData training_data;
  example_1.weight = weight;
  training_data.push_back(example_1);
  // Push many |example_2|'s, which will win without the weights.
  training_data.push_back(example_2);
  training_data.push_back(example_2);
  training_data.push_back(example_2);
  training_data.push_back(example_2);

  // Create a weighed set with |weight| for each example's weight.
  EXPECT_FALSE(training_data.is_unweighted());
  auto model = trainer_.Train(task_, training_data);

  // The singular max should be example_1.
  TargetDistribution distribution =
      model->PredictDistribution(example_1.features);
  TargetValue predicted_value;
  EXPECT_TRUE(distribution.FindSingularMax(&predicted_value));
  EXPECT_EQ(predicted_value, example_1.target_value);
}

INSTANTIATE_TEST_CASE_P(ExtraTreesTest,
                        ExtraTreesTest,
                        testing::ValuesIn({LearningTask::Ordering::kUnordered,
                                           LearningTask::Ordering::kNumeric}));

}  // namespace learning
}  // namespace media
