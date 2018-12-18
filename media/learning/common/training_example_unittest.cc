// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/common/training_example.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace learning {

class LearnerTrainingExampleTest : public testing::Test {};

TEST_F(LearnerTrainingExampleTest, InitListWorks) {
  const int kFeature1 = 123;
  const int kFeature2 = 456;
  std::vector<FeatureValue> features = {FeatureValue(kFeature1),
                                        FeatureValue(kFeature2)};
  TargetValue target(789);
  TrainingExample example({FeatureValue(kFeature1), FeatureValue(kFeature2)},
                          target);

  EXPECT_EQ(example.features, features);
  EXPECT_EQ(example.target_value, target);
}

TEST_F(LearnerTrainingExampleTest, CopyConstructionWorks) {
  TrainingExample example_1({FeatureValue(123), FeatureValue(456)},
                            TargetValue(789));
  TrainingExample example_2(example_1);

  EXPECT_EQ(example_1, example_2);
}

TEST_F(LearnerTrainingExampleTest, MoveConstructionWorks) {
  TrainingExample example_1({FeatureValue(123), FeatureValue(456)},
                            TargetValue(789));

  TrainingExample example_1_copy(example_1);
  TrainingExample example_1_move(std::move(example_1));

  EXPECT_EQ(example_1_copy, example_1_move);
  EXPECT_NE(example_1_copy, example_1);
}

TEST_F(LearnerTrainingExampleTest, EqualExamplesCompareAsEqual) {
  const int kFeature1 = 123;
  const int kFeature2 = 456;
  TargetValue target(789);
  TrainingExample example_1({FeatureValue(kFeature1), FeatureValue(kFeature2)},
                            target);
  TrainingExample example_2({FeatureValue(kFeature1), FeatureValue(kFeature2)},
                            target);
  // Verify both that == and != work.
  EXPECT_EQ(example_1, example_2);
  EXPECT_FALSE(example_1 != example_2);
  // Also insist that equal examples are not less.
  EXPECT_FALSE(example_1 < example_2);
  EXPECT_FALSE(example_2 < example_1);
}

TEST_F(LearnerTrainingExampleTest, UnequalFeaturesCompareAsUnequal) {
  const int kFeature1 = 123;
  const int kFeature2 = 456;
  TargetValue target(789);
  TrainingExample example_1({FeatureValue(kFeature1), FeatureValue(kFeature1)},
                            target);
  TrainingExample example_2({FeatureValue(kFeature2), FeatureValue(kFeature2)},
                            target);
  EXPECT_TRUE(example_1 != example_2);
  EXPECT_FALSE(example_1 == example_2);
  // We don't care which way is <, but we do care that one is less than the
  // other but not both.
  EXPECT_NE((example_1 < example_2), (example_2 < example_1));
}

TEST_F(LearnerTrainingExampleTest, WeightDoesntChangeExampleEquality) {
  const int kFeature1 = 123;
  TargetValue target(789);
  TrainingExample example_1({FeatureValue(kFeature1)}, target);
  TrainingExample example_2 = example_1;

  // Set the weights to be unequal.  This should not affect the comparison.
  example_1.weight = 10u;
  example_2.weight = 20u;

  // Verify both that == and != ignore weights.
  EXPECT_EQ(example_1, example_2);
  EXPECT_FALSE(example_1 != example_2);
  // Also insist that equal examples are not less.
  EXPECT_FALSE(example_1 < example_2);
  EXPECT_FALSE(example_2 < example_1);
}

TEST_F(LearnerTrainingExampleTest, ExampleAssignmentCopiesWeights) {
  // While comparisons ignore weights, copy / assign should not.
  const int kFeature1 = 123;
  TargetValue target(789);
  TrainingExample example_1({FeatureValue(kFeature1)}, target);
  example_1.weight = 10u;

  // Copy-assignment.
  TrainingExample example_2;
  example_2 = example_1;
  EXPECT_EQ(example_1, example_2);
  EXPECT_EQ(example_1.weight, example_2.weight);

  // Copy-construction.
  TrainingExample example_3(example_1);
  EXPECT_EQ(example_1, example_3);
  EXPECT_EQ(example_1.weight, example_3.weight);

  // Move-assignment.
  TrainingExample example_4;
  example_4 = std::move(example_2);
  EXPECT_EQ(example_1, example_4);
  EXPECT_EQ(example_1.weight, example_4.weight);

  // Move-construction.
  TrainingExample example_5(std::move(example_3));
  EXPECT_EQ(example_1, example_5);
  EXPECT_EQ(example_1.weight, example_5.weight);
}

TEST_F(LearnerTrainingExampleTest, UnequalTargetsCompareAsUnequal) {
  const int kFeature1 = 123;
  const int kFeature2 = 456;
  TrainingExample example_1({FeatureValue(kFeature1), FeatureValue(kFeature1)},
                            TargetValue(789));
  TrainingExample example_2({FeatureValue(kFeature2), FeatureValue(kFeature2)},
                            TargetValue(987));
  EXPECT_TRUE(example_1 != example_2);
  EXPECT_FALSE(example_1 == example_2);
  // Exactly one should be less than the other, but we don't care which one.
  EXPECT_TRUE((example_1 < example_2) ^ (example_2 < example_1));
}

TEST_F(LearnerTrainingExampleTest, OrderingIsTransitive) {
  // Verify that ordering is transitive.  We don't particularly care what the
  // ordering is, otherwise.

  const FeatureValue kFeature1(123);
  const FeatureValue kFeature2(456);
  const FeatureValue kTarget1(789);
  const FeatureValue kTarget2(987);
  std::vector<TrainingExample> examples;
  examples.push_back(TrainingExample({kFeature1}, kTarget1));
  examples.push_back(TrainingExample({kFeature1}, kTarget2));
  examples.push_back(TrainingExample({kFeature2}, kTarget1));
  examples.push_back(TrainingExample({kFeature2}, kTarget2));
  examples.push_back(TrainingExample({kFeature1, kFeature2}, kTarget1));
  examples.push_back(TrainingExample({kFeature1, kFeature2}, kTarget2));
  examples.push_back(TrainingExample({kFeature2, kFeature1}, kTarget1));
  examples.push_back(TrainingExample({kFeature2, kFeature1}, kTarget2));

  // Sort, and make sure that it ends up totally ordered.
  std::sort(examples.begin(), examples.end());
  for (auto outer = examples.begin(); outer != examples.end(); outer++) {
    for (auto inner = outer + 1; inner != examples.end(); inner++) {
      EXPECT_TRUE(*outer < *inner);
      EXPECT_FALSE(*inner < *outer);
    }
  }
}

TEST_F(LearnerTrainingExampleTest, UnweightedTrainingDataPushBack) {
  // Test that pushing examples from unweighted storage into TrainingData works.
  TrainingData training_data;
  EXPECT_EQ(training_data.total_weight(), 0u);
  EXPECT_TRUE(training_data.empty());

  TrainingExample example({FeatureValue(123)}, TargetValue(789));
  training_data.push_back(example);
  EXPECT_EQ(training_data.total_weight(), 1u);
  EXPECT_FALSE(training_data.empty());
  EXPECT_TRUE(training_data.is_unweighted());
  EXPECT_EQ(training_data[0], example);
}

TEST_F(LearnerTrainingExampleTest, WeightedTrainingDataPushBack) {
  // Test that pushing examples from weighted storage into TrainingData works.
  TrainingData training_data;
  EXPECT_EQ(training_data.total_weight(), 0u);
  EXPECT_TRUE(training_data.empty());

  TrainingExample example({FeatureValue(123)}, TargetValue(789));
  const weight_t weight(10);
  example.weight = weight;
  training_data.push_back(example);
  training_data.push_back(example);

  EXPECT_EQ(training_data.total_weight(), weight * 2);
  EXPECT_FALSE(training_data.empty());
  EXPECT_FALSE(training_data.is_unweighted());
  EXPECT_EQ(training_data[0], example);
}

}  // namespace learning
}  // namespace media
