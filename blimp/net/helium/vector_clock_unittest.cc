// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/helium/vector_clock.h"

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blimp {
namespace {

class VectorClockComparisonTest
    : public ::testing::TestWithParam<
          std::tuple<VectorClock, VectorClock, VectorClock::Comparison>> {
 public:
  VectorClockComparisonTest() {}
  ~VectorClockComparisonTest() override {}
};

TEST_P(VectorClockComparisonTest, CompareTo) {
  auto param = GetParam();
  VectorClock v1 = std::get<0>(param);
  VectorClock v2 = std::get<1>(param);
  VectorClock::Comparison expected = std::get<2>(param);
  EXPECT_EQ(expected, v1.CompareTo(v2));
}

INSTANTIATE_TEST_CASE_P(
    LessThan,
    VectorClockComparisonTest,
    ::testing::Values(std::make_tuple(VectorClock(1, 2),
                                      VectorClock(1, 3),
                                      VectorClock::Comparison::LessThan)));

INSTANTIATE_TEST_CASE_P(
    GreaterThan,
    VectorClockComparisonTest,
    ::testing::Values(std::make_tuple(VectorClock(1, 3),
                                      VectorClock(1, 2),
                                      VectorClock::Comparison::GreaterThan),
                      std::make_tuple(VectorClock(2, 2),
                                      VectorClock(1, 2),
                                      VectorClock::Comparison::GreaterThan)));

INSTANTIATE_TEST_CASE_P(
    Conflict,
    VectorClockComparisonTest,
    ::testing::Values(std::make_tuple(VectorClock(1, 2),
                                      VectorClock(0, 1),
                                      VectorClock::Comparison::Conflict),
                      std::make_tuple(VectorClock(1, 2),
                                      VectorClock(0, 3),
                                      VectorClock::Comparison::Conflict)));

INSTANTIATE_TEST_CASE_P(
    EqualTo,
    VectorClockComparisonTest,
    ::testing::Values(std::make_tuple(VectorClock(1, 1),
                                      VectorClock(1, 1),
                                      VectorClock::Comparison::EqualTo),
                      std::make_tuple(VectorClock(2, 3),
                                      VectorClock(2, 3),
                                      VectorClock::Comparison::EqualTo),
                      std::make_tuple(VectorClock(3, 2),
                                      VectorClock(3, 2),
                                      VectorClock::Comparison::EqualTo)));

class VectorClockTest : public testing::Test {
 public:
  VectorClockTest() {}
  ~VectorClockTest() override {}

 protected:
  void CheckCumulativeMerge(const VectorClock& v1,
                            const VectorClock& v2,
                            const VectorClock& expected) {
    // Compute the merge of v1 and v2
    VectorClock r1 = v1.MergeWith(v2);
    EXPECT_EQ(expected.local_revision(), r1.local_revision());
    EXPECT_EQ(expected.remote_revision(), r1.remote_revision());

    // Compute the merge of v2 and v1
    VectorClock r2 = v2.MergeWith(v1);
    EXPECT_EQ(expected.local_revision(), r2.local_revision());
    EXPECT_EQ(expected.remote_revision(), r2.remote_revision());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(VectorClockTest);
};

TEST_F(VectorClockTest, IncrementLocal1) {
  VectorClock v(0, 0);
  v.IncrementLocal();
  EXPECT_EQ(1U, v.local_revision());
  EXPECT_EQ(0U, v.remote_revision());
}

TEST_F(VectorClockTest, IncrementLocal2) {
  VectorClock v(4, 5);
  v.IncrementLocal();
  EXPECT_EQ(5U, v.local_revision());
  EXPECT_EQ(5U, v.remote_revision());
}

TEST_F(VectorClockTest, MergeLocalEqualRemoteSmaller) {
  VectorClock v1(1, 2);
  VectorClock v2(1, 4);

  VectorClock expected(1, 4);
  CheckCumulativeMerge(v1, v2, expected);
}

TEST_F(VectorClockTest, MergeLocalSmallerRemoteEqual) {
  VectorClock v1(1, 4);
  VectorClock v2(2, 4);

  VectorClock expected(2, 4);
  CheckCumulativeMerge(v1, v2, expected);
}

TEST_F(VectorClockTest, MergeLocalSmallerRemoteSmaller) {
  VectorClock v1(1, 2);
  VectorClock v2(3, 4);

  VectorClock expected(3, 4);
  CheckCumulativeMerge(v1, v2, expected);
}

TEST_F(VectorClockTest, MergeLocalSmallerRemoteGreater) {
  VectorClock v1(1, 4);
  VectorClock v2(3, 2);

  VectorClock expected(3, 4);
  CheckCumulativeMerge(v1, v2, expected);
}

}  // namespace
}  // namespace blimp
