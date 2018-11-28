// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/impl/target_distribution.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace learning {

class TargetDistributionTest : public testing::Test {
 public:
  TargetDistributionTest() : value_1(123), value_2(456), value_3(789) {}

  TargetDistribution distribution_;

  TargetValue value_1;
  const int counts_1 = 100;

  TargetValue value_2;
  const int counts_2 = 10;

  TargetValue value_3;
};

TEST_F(TargetDistributionTest, EmptyTargetDistributionHasZeroCounts) {
  EXPECT_EQ(distribution_.total_counts(), 0);
}

TEST_F(TargetDistributionTest, AddingCountsWorks) {
  distribution_[value_1] = counts_1;
  EXPECT_EQ(distribution_.total_counts(), counts_1);
  EXPECT_EQ(distribution_[value_1], counts_1);
  distribution_[value_1] += counts_1;
  EXPECT_EQ(distribution_.total_counts(), counts_1 * 2);
  EXPECT_EQ(distribution_[value_1], counts_1 * 2);
}

TEST_F(TargetDistributionTest, MultipleValuesAreSeparate) {
  distribution_[value_1] = counts_1;
  distribution_[value_2] = counts_2;
  EXPECT_EQ(distribution_.total_counts(), counts_1 + counts_2);
  EXPECT_EQ(distribution_[value_1], counts_1);
  EXPECT_EQ(distribution_[value_2], counts_2);
}

TEST_F(TargetDistributionTest, AddingTargetValues) {
  distribution_ += value_1;
  EXPECT_EQ(distribution_.total_counts(), 1);
  EXPECT_EQ(distribution_[value_1], 1);
  EXPECT_EQ(distribution_[value_2], 0);

  distribution_ += value_1;
  EXPECT_EQ(distribution_.total_counts(), 2);
  EXPECT_EQ(distribution_[value_1], 2);
  EXPECT_EQ(distribution_[value_2], 0);

  distribution_ += value_2;
  EXPECT_EQ(distribution_.total_counts(), 3);
  EXPECT_EQ(distribution_[value_1], 2);
  EXPECT_EQ(distribution_[value_2], 1);
}

TEST_F(TargetDistributionTest, AddingTargetDistributions) {
  distribution_[value_1] = counts_1;

  TargetDistribution rhs;
  rhs[value_2] = counts_2;

  distribution_ += rhs;

  EXPECT_EQ(distribution_.total_counts(), counts_1 + counts_2);
  EXPECT_EQ(distribution_[value_1], counts_1);
  EXPECT_EQ(distribution_[value_2], counts_2);
}

TEST_F(TargetDistributionTest, FindSingularMaxFindsTheSingularMax) {
  distribution_[value_1] = counts_1;
  distribution_[value_2] = counts_2;
  ASSERT_TRUE(counts_1 > counts_2);

  TargetValue max_value(0);
  int max_counts = 0;
  EXPECT_TRUE(distribution_.FindSingularMax(&max_value, &max_counts));
  EXPECT_EQ(max_value, value_1);
  EXPECT_EQ(max_counts, counts_1);
}

TEST_F(TargetDistributionTest,
       FindSingularMaxFindsTheSingularMaxAlternateOrder) {
  // Switch the order, to handle sorting in different directions.
  distribution_[value_1] = counts_2;
  distribution_[value_2] = counts_1;
  ASSERT_TRUE(counts_1 > counts_2);

  TargetValue max_value(0);
  int max_counts = 0;
  EXPECT_TRUE(distribution_.FindSingularMax(&max_value, &max_counts));
  EXPECT_EQ(max_value, value_2);
  EXPECT_EQ(max_counts, counts_1);
}

TEST_F(TargetDistributionTest, FindSingularMaxReturnsFalsForNonSingularMax) {
  distribution_[value_1] = counts_1;
  distribution_[value_2] = counts_1;

  TargetValue max_value(0);
  int max_counts = 0;
  EXPECT_FALSE(distribution_.FindSingularMax(&max_value, &max_counts));
}

TEST_F(TargetDistributionTest, FindSingularMaxIgnoresNonSingularNonMax) {
  distribution_[value_1] = counts_1;
  // |value_2| and |value_3| are tied, but not the max.
  distribution_[value_2] = counts_2;
  distribution_[value_3] = counts_2;
  ASSERT_TRUE(counts_1 > counts_2);

  TargetValue max_value(0);
  int max_counts = 0;
  EXPECT_TRUE(distribution_.FindSingularMax(&max_value, &max_counts));
  EXPECT_EQ(max_value, value_1);
  EXPECT_EQ(max_counts, counts_1);
}

TEST_F(TargetDistributionTest, FindSingularMaxDoesntRequireCounts) {
  distribution_[value_1] = counts_1;

  TargetValue max_value(0);
  EXPECT_TRUE(distribution_.FindSingularMax(&max_value));
  EXPECT_EQ(max_value, value_1);
}

}  // namespace learning
}  // namespace media
