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

TEST_P(ExtraTreesTest, RegressionWorks) {
  // Create a training set with unseparable data, but give one of them a large
  // weight.  See if that one wins.
  SetupFeatures(2);
  TrainingExample example_1({FeatureValue(1), FeatureValue(123)},
                            TargetValue(1));
  TrainingExample example_1_a({FeatureValue(1), FeatureValue(123)},
                              TargetValue(5));
  TrainingExample example_2({FeatureValue(1), FeatureValue(456)},
                            TargetValue(20));
  TrainingExample example_2_a({FeatureValue(1), FeatureValue(456)},
                              TargetValue(25));
  TrainingData training_data;
  example_1.weight = 100;
  training_data.push_back(example_1);
  training_data.push_back(example_1_a);
  example_2.weight = 100;
  training_data.push_back(example_2);
  training_data.push_back(example_2_a);

  task_.target_description.ordering = LearningTask::Ordering::kNumeric;

  // Create a weighed set with |weight| for each example's weight.
  auto model = trainer_.Train(task_, training_data);

  // Make sure that the results are in the right range.
  TargetDistribution distribution =
      model->PredictDistribution(example_1.features);
  EXPECT_GT(distribution.Average(), example_1.target_value.value() * 0.95);
  EXPECT_LT(distribution.Average(), example_1.target_value.value() * 1.05);
  distribution = model->PredictDistribution(example_2.features);
  EXPECT_GT(distribution.Average(), example_2.target_value.value() * 0.95);
  EXPECT_LT(distribution.Average(), example_2.target_value.value() * 1.05);
}

TEST_P(ExtraTreesTest, RegressionVsBinaryClassification) {
  // Create a binary classification task and a regression task that are roughly
  // the same.  Verify that the results are the same, too.  In particular, for
  // each set of features, we choose a regression target |pct| between 0 and
  // 100.  For the corresponding binary classification problem, we add |pct|
  // true instances, and 100-|pct| false instances.  The predicted averages
  // should be roughly the same.
  SetupFeatures(3);
  TrainingData c_data, r_data;

  std::set<TrainingExample> r_examples;
  for (size_t i = 0; i < 4 * 4 * 4; i++) {
    FeatureValue f1(i & 3);
    FeatureValue f2((i >> 2) & 3);
    FeatureValue f3((i >> 4) & 3);
    int pct = (100 * (f1.value() + f2.value() + f3.value())) / 9;
    TrainingExample e({f1, f2, f3}, TargetValue(0));

    // TODO(liberato): Consider adding noise, and verifying that the model
    // predictions are roughly the same as each other, rather than the same as
    // the currently noise-free target.

    // Push some number of false and some number of true instances that is in
    // the right ratio for |pct|.  We add 100's instead of 1's so that it's
    // scaled to the same range as the regression targets.
    e.weight = 100 - pct;
    if (e.weight > 0)
      c_data.push_back(e);
    e.target_value = TargetValue(100);
    e.weight = pct;
    if (e.weight > 0)
      c_data.push_back(e);

    // For the regression data, add an example with |pct| directly.  Also save
    // it so that we can look up the right answer below.
    TrainingExample r_example(TrainingExample({f1, f2, f3}, TargetValue(pct)));
    r_examples.insert(r_example);
    r_data.push_back(r_example);
  }

  // Train a model on the binary classification task and the regression task.
  auto c_model = trainer_.Train(task_, c_data);
  task_.target_description.ordering = LearningTask::Ordering::kNumeric;
  auto r_model = trainer_.Train(task_, r_data);

  // Verify that, for all feature combinations, the models roughly agree.  Since
  // the data is separable, it probably should be exact.
  for (auto& r_example : r_examples) {
    const FeatureVector& fv = r_example.features;
    TargetDistribution c_dist = c_model->PredictDistribution(fv);
    EXPECT_LE(c_dist.Average(), r_example.target_value.value() * 1.05);
    EXPECT_GE(c_dist.Average(), r_example.target_value.value() * 0.95);
    TargetDistribution r_dist = r_model->PredictDistribution(fv);
    EXPECT_LE(r_dist.Average(), r_example.target_value.value() * 1.05);
    EXPECT_GE(r_dist.Average(), r_example.target_value.value() * 0.95);
  }
}

INSTANTIATE_TEST_CASE_P(ExtraTreesTest,
                        ExtraTreesTest,
                        testing::ValuesIn({LearningTask::Ordering::kUnordered,
                                           LearningTask::Ordering::kNumeric}));

}  // namespace learning
}  // namespace media
