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

TEST_F(LearnerTrainingExampleTest, UnweightedStoragePushBack) {
  // Test that pushing unweighted examples into storage works.
  TrainingExample example({FeatureValue(123)}, TargetValue(789));
  scoped_refptr<TrainingDataStorage> storage =
      base::MakeRefCounted<TrainingDataStorage>();
  EXPECT_EQ(storage->begin(), storage->end());
  storage->push_back(example);
  EXPECT_NE(storage->begin(), storage->end());
  EXPECT_EQ(++storage->begin(), storage->end());
  EXPECT_EQ(*storage->begin()->example(), example);
  EXPECT_EQ(storage->begin()->weight(), 1u);
  EXPECT_EQ(storage->size(), 1u);
  EXPECT_EQ(storage->total_weight(), 1u);
}

TEST_F(LearnerTrainingExampleTest, WeightedStoragePushBack) {
  // Test that pushing weighted examples into storage works.
  TrainingExample example({FeatureValue(123)}, TargetValue(789));
  scoped_refptr<TrainingDataStorage> storage =
      base::MakeRefCounted<TrainingDataStorage>();
  EXPECT_EQ(storage->begin(), storage->end());
  const WeightedExample::weight_t weight(10);
  storage->push_back(example, weight);
  EXPECT_NE(storage->begin(), storage->end());
  EXPECT_EQ(++storage->begin(), storage->end());
  EXPECT_EQ(*storage->begin()->example(), example);
  EXPECT_EQ(storage->begin()->weight(), weight);
  EXPECT_EQ(storage->size(), 1u);
  EXPECT_EQ(storage->total_weight(), weight);
}

TEST_F(LearnerTrainingExampleTest, UnweightedTrainingDataPushBack) {
  // Test that pushing examples from unweighted storage into TrainingData works.
  TrainingExample example({FeatureValue(123)}, TargetValue(789));
  scoped_refptr<TrainingDataStorage> storage =
      base::MakeRefCounted<TrainingDataStorage>();
  storage->push_back(example);

  TrainingData training_data(storage);
  EXPECT_EQ(training_data.total_weight(), 0u);
  EXPECT_TRUE(training_data.empty());
  training_data.push_back(*storage->begin());
  EXPECT_EQ(training_data.total_weight(), 1u);
  EXPECT_FALSE(training_data.empty());
  EXPECT_TRUE(training_data.is_unweighted());
  EXPECT_EQ(training_data.begin()->example(), storage->begin()->example());
  EXPECT_EQ(training_data.begin()->weight(), 1u);
}

TEST_F(LearnerTrainingExampleTest, WeightedTrainingDataPushBack) {
  // Test that pushing examples from weighted storage into TrainingData works.
  TrainingExample example({FeatureValue(123)}, TargetValue(789));
  const WeightedExample::weight_t weight(10);
  scoped_refptr<TrainingDataStorage> storage =
      base::MakeRefCounted<TrainingDataStorage>();
  storage->push_back(example, weight);

  TrainingData training_data(storage);
  EXPECT_EQ(training_data.total_weight(), 0u);
  EXPECT_TRUE(training_data.empty());
  training_data.push_back(*storage->begin());
  EXPECT_EQ(training_data.total_weight(), weight);
  EXPECT_FALSE(training_data.empty());
  EXPECT_FALSE(training_data.is_unweighted());
  EXPECT_EQ(training_data.begin()->example(), storage->begin()->example());
  EXPECT_EQ(training_data.begin()->weight(), weight);
}

TEST_F(LearnerTrainingExampleTest, TrainingDataConstructWithRange) {
  scoped_refptr<TrainingDataStorage> storage =
      base::MakeRefCounted<TrainingDataStorage>();

  // Add one weighted and one unweighted example.
  TrainingExample unweighted_example({FeatureValue(123)}, TargetValue(789));
  storage->push_back(unweighted_example);
  TrainingExample weighted_example({FeatureValue(456)}, TargetValue(789));
  const WeightedExample::weight_t weight(10);
  storage->push_back(weighted_example, weight);

  TrainingData training_data(storage, storage->begin(), storage->end());
  EXPECT_EQ(training_data.total_weight(), 1u + weight);
  EXPECT_FALSE(training_data.empty());
  EXPECT_EQ(training_data.begin()->example(), storage->begin()->example());
  EXPECT_EQ(training_data.begin()->weight(), 1u);
  EXPECT_EQ((++(training_data.begin()))->example(),
            (++(storage->begin()))->example());
  EXPECT_EQ((++(training_data.begin()))->weight(), weight);
}

TEST_F(LearnerTrainingExampleTest, AddWeighedExamplesToTrainingData) {
  // Add examples to TrainingData with different weights than are in the
  // storage, and verify that the TrainingData weights are used.
  TrainingExample example({FeatureValue(123)}, TargetValue(789));
  scoped_refptr<TrainingDataStorage> storage =
      base::MakeRefCounted<TrainingDataStorage>();
  storage->push_back(example);

  TrainingData training_data(storage);
  const WeightedExample::weight_t weight(100);
  training_data.push_back(WeightedExample(storage->begin()->example(), weight));
  EXPECT_EQ(training_data.total_weight(), weight);
  EXPECT_FALSE(training_data.is_unweighted());
}

}  // namespace learning
}  // namespace media
