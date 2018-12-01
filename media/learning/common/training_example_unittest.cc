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
}

TEST_F(LearnerTrainingExampleTest, UnequalFeaturesCompareAsUnequal) {
  const int kFeature1 = 123;
  const int kFeature2 = 456;
  TargetValue target(789);
  TrainingExample example_1({FeatureValue(kFeature1), FeatureValue(kFeature1)},
                            target);
  TrainingExample example_2({FeatureValue(kFeature2), FeatureValue(kFeature2)},
                            target);
  EXPECT_NE(example_1, example_2);
  EXPECT_FALSE(example_1 == example_2);
}

TEST_F(LearnerTrainingExampleTest, UnequalTargetsCompareAsUnequal) {
  const int kFeature1 = 123;
  const int kFeature2 = 456;
  TrainingExample example_1({FeatureValue(kFeature1), FeatureValue(kFeature1)},
                            TargetValue(789));
  TrainingExample example_2({FeatureValue(kFeature2), FeatureValue(kFeature2)},
                            TargetValue(987));
  EXPECT_NE(example_1, example_2);
  EXPECT_FALSE(example_1 == example_2);
}

TEST_F(LearnerTrainingExampleTest, StoragePushBack) {
  TrainingExample example({FeatureValue(123)}, TargetValue(789));
  scoped_refptr<TrainingDataStorage> storage =
      base::MakeRefCounted<TrainingDataStorage>();
  EXPECT_EQ(storage->begin(), storage->end());
  storage->push_back(example);
  EXPECT_NE(storage->begin(), storage->end());
  EXPECT_EQ(++storage->begin(), storage->end());
  EXPECT_EQ(*storage->begin(), example);
}

TEST_F(LearnerTrainingExampleTest, TrainingDataPushBack) {
  TrainingExample example({FeatureValue(123)}, TargetValue(789));
  scoped_refptr<TrainingDataStorage> storage =
      base::MakeRefCounted<TrainingDataStorage>();
  storage->push_back(example);

  TrainingData training_data(storage);
  EXPECT_EQ(training_data.size(), 0u);
  EXPECT_TRUE(training_data.empty());
  training_data.push_back(&(*storage->begin()));
  EXPECT_EQ(training_data.size(), 1u);
  EXPECT_FALSE(training_data.empty());
  EXPECT_EQ(*training_data.begin(), &(*storage->begin()));
  EXPECT_EQ(training_data[0], &(*storage->begin()));
}

TEST_F(LearnerTrainingExampleTest, TrainingDataConstructWithRange) {
  TrainingExample example({FeatureValue(123)}, TargetValue(789));
  scoped_refptr<TrainingDataStorage> storage =
      base::MakeRefCounted<TrainingDataStorage>();
  storage->push_back(example);

  TrainingData training_data(storage, storage->begin(), storage->end());
  EXPECT_EQ(training_data.size(), 1u);
  EXPECT_FALSE(training_data.empty());
  EXPECT_EQ(*training_data.begin(), &(*storage->begin()));
  EXPECT_EQ(training_data[0], &(*storage->begin()));
}

}  // namespace learning
}  // namespace media
