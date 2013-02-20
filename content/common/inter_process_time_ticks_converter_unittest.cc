// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NOTE: All of these tests have been disabled due to the
// InterProcessTimeTicksConverter being disabled due to bug 174170.

#include "base/time.h"
#include "content/common/inter_process_time_ticks_converter.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeTicks;

namespace content {

namespace {

struct TestParams {
  int64 local_lower_bound;
  int64 remote_lower_bound;
  int64 remote_upper_bound;
  int64 local_upper_bound;
  int64 test_time;
  int64 test_delta;
};

struct TestResults {
  int64 result_time;
  int32 result_delta;
};

TestResults RunTest(const TestParams& params) {
  TimeTicks local_lower_bound = TimeTicks::FromInternalValue(
      params.local_lower_bound);
  TimeTicks local_upper_bound = TimeTicks::FromInternalValue(
      params.local_upper_bound);
  TimeTicks remote_lower_bound = TimeTicks::FromInternalValue(
      params.remote_lower_bound);
  TimeTicks remote_upper_bound = TimeTicks::FromInternalValue(
      params.remote_upper_bound);
  TimeTicks test_time = TimeTicks::FromInternalValue(params.test_time);

  InterProcessTimeTicksConverter converter(
      LocalTimeTicks::FromTimeTicks(local_lower_bound),
      LocalTimeTicks::FromTimeTicks(local_upper_bound),
      RemoteTimeTicks::FromTimeTicks(remote_lower_bound),
      RemoteTimeTicks::FromTimeTicks(remote_upper_bound));

  TestResults results;
  results.result_time = converter.ToLocalTimeTicks(
      RemoteTimeTicks::FromTimeTicks(
          test_time)).ToTimeTicks().ToInternalValue();
  results.result_delta = converter.ToLocalTimeDelta(
      RemoteTimeDelta::FromRawDelta(params.test_delta)).ToInt32();
  return results;
}

TEST(InterProcessTimeTicksConverterTest, DISABLED_NoSkew) {
  // All times are monotonic and centered, so no adjustment should occur.
  TestParams p;
  p.local_lower_bound = 0;
  p.remote_lower_bound = 1;
  p.remote_upper_bound = 4;
  p.local_upper_bound = 5;
  p.test_time = 2;
  p.test_delta = 1;
  TestResults results = RunTest(p);
  EXPECT_EQ(2, results.result_time);
  EXPECT_EQ(1, results.result_delta);
}

TEST(InterProcessTimeTicksConverterTest, DISABLED_OffsetMidpoints) {
  // All times are monotonic, but not centered. Adjust the |remote_*| times so
  // they are centered within the |local_*| times.
  TestParams p;
  p.local_lower_bound = 0;
  p.remote_lower_bound = 2;
  p.remote_upper_bound = 5;
  p.local_upper_bound = 5;
  p.test_time = 3;
  p.test_delta = 1;
  TestResults results = RunTest(p);
  EXPECT_EQ(2, results.result_time);
  EXPECT_EQ(1, results.result_delta);
}

TEST(InterProcessTimeTicksConverterTest, DISABLED_DoubleEndedSkew) {
  // |remote_lower_bound| occurs before |local_lower_bound| and
  // |remote_upper_bound| occurs after |local_upper_bound|. We must adjust both
  // bounds and scale down the delta. |test_time| is on the midpoint, so it
  // doesn't change. The ratio of local time to network time is 1:2, so we scale
  // |test_delta| to half.
  TestParams p;
  p.local_lower_bound = 2;
  p.remote_lower_bound = 0;
  p.remote_upper_bound = 8;
  p.local_upper_bound = 6;
  p.test_time = 4;
  p.test_delta = 2;
  TestResults results = RunTest(p);
  EXPECT_EQ(4, results.result_time);
  EXPECT_EQ(1, results.result_delta);
}

TEST(InterProcessTimeTicksConverterTest, DISABLED_FrontEndSkew) {
  // |remote_upper_bound| is coherent, but |remote_lower_bound| is not. So we
  // adjust the lower bound and move |test_time| out. The scale factor is 2:3,
  // but since we use integers, the numbers truncate from 3.33 to 3 and 1.33
  // to 1.
  TestParams p;
  p.local_lower_bound = 2;
  p.remote_lower_bound = 0;
  p.remote_upper_bound = 6;
  p.local_upper_bound = 6;
  p.test_time = 2;
  p.test_delta = 2;
  TestResults results = RunTest(p);
  EXPECT_EQ(3, results.result_time);
  EXPECT_EQ(1, results.result_delta);
}

TEST(InterProcessTimeTicksConverterTest, DISABLED_BackEndSkew) {
  // Like the previous test, but |remote_lower_bound| is coherent and
  // |remote_upper_bound| is skewed.
  TestParams p;
  p.local_lower_bound = 0;
  p.remote_lower_bound = 0;
  p.remote_upper_bound = 6;
  p.local_upper_bound = 4;
  p.test_time = 2;
  p.test_delta = 2;
  TestResults results = RunTest(p);
  EXPECT_EQ(1, results.result_time);
  EXPECT_EQ(1, results.result_delta);
}

TEST(InterProcessTimeTicksConverterTest, DISABLED_Instantaneous) {
  // The bounds are all okay, but the |remote_lower_bound| and
  // |remote_upper_bound| have the same value. No adjustments should be made and
  // no divide-by-zero errors should occur.
  TestParams p;
  p.local_lower_bound = 0;
  p.remote_lower_bound = 1;
  p.remote_upper_bound = 1;
  p.local_upper_bound = 2;
  p.test_time = 1;
  p.test_delta = 0;
  TestResults results = RunTest(p);
  EXPECT_EQ(1, results.result_time);
  EXPECT_EQ(0, results.result_delta);
}

TEST(InterProcessTimeTicksConverterTest, DISABLED_OffsetInstantaneous) {
  // The bounds are all okay, but the |remote_lower_bound| and
  // |remote_upper_bound| have the same value and are offset from the midpoint
  // of |local_lower_bound| and |local_upper_bound|. An offset should be applied
  // to make the midpoints line up.
  TestParams p;
  p.local_lower_bound = 0;
  p.remote_lower_bound = 2;
  p.remote_upper_bound = 2;
  p.local_upper_bound = 2;
  p.test_time = 2;
  p.test_delta = 0;
  TestResults results = RunTest(p);
  EXPECT_EQ(1, results.result_time);
  EXPECT_EQ(0, results.result_delta);
}

TEST(InterProcessTimeTicksConverterTest, DISABLED_DisjointInstantaneous) {
  // |local_lower_bound| and |local_upper_bound| are the same. No matter what
  // the other values are, they must fit within [local_lower_bound,
  // local_upper_bound].  So, all of the values should be adjusted so they are
  // exactly that value.
  TestParams p;
  p.local_lower_bound = 1;
  p.remote_lower_bound = 2;
  p.remote_upper_bound = 2;
  p.local_upper_bound = 1;
  p.test_time = 2;
  p.test_delta = 0;
  TestResults results = RunTest(p);
  EXPECT_EQ(1, results.result_time);
  EXPECT_EQ(0, results.result_delta);
}

TEST(InterProcessTimeTicksConverterTest, DISABLED_RoundingNearEdges) {
  // Verify that rounding never causes a value to appear outside the given
  // |local_*| range.
  const int kMaxRange = 100;
  for (int i = 0; i < kMaxRange; ++i) {
    for (int j = 0; j < kMaxRange; ++j) {
      TestParams p;
      p.local_lower_bound = 0;
      p.remote_lower_bound = 0;
      p.remote_upper_bound = j;
      p.local_upper_bound = i;
      p.test_time = 0;
      p.test_delta = j;
      TestResults results = RunTest(p);
      EXPECT_LE(0, results.result_time);
      EXPECT_GE(i, results.result_delta);
    }
  }
}

}  // anonymous namespace

}  // namespace content
