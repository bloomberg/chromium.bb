// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/rand_util.h"

#include <ctime>
#include <random>

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_test.h"

namespace {

// Deprecated. Needed to benchmark the performance of the previous
// implementation of base::RandInt().
int RandIntDeprecated(int min, int max) {
  DCHECK_LE(min, max);

  uint64_t range = static_cast<uint64_t>(max) - min + 1;
  // |range| is at most UINT_MAX + 1, so the result of RandGenerator(range)
  // is at most UINT_MAX.  Hence it's safe to cast it from uint64_t to int64_t.
  int result =
      static_cast<int>(min + static_cast<int64_t>(base::RandGenerator(range)));

  DCHECK_GE(result, min);
  DCHECK_LE(result, max);

  return result;
}

}  // namespace

namespace base {

// Logs the average number of calls per second to base::RandInt(). Time is
// measured every 1000 calls and the total number of calls is normalized to
// reflect the average number of calls  per second.
TEST(RandUtilPerfTest, AverageNumberOfCallsPerSecondToGetRandInt) {
  base::TimeTicks start = base::TimeTicks::Now();
  base::TimeTicks now;

  size_t num_rounds = 0;
  constexpr int kBatchSize = 1000;

  do {
    int num_calls = 0;
    while (num_calls < kBatchSize) {
      RandInt(0, 1000);
      ++num_calls;
    }
    now = base::TimeTicks::Now();
    ++num_rounds;
  } while (now - start < base::TimeDelta::FromSeconds(1));

  perf_test::PrintResult(
      "Task", " (Time is measured after every 1000 function calls)",
      "Average number of calls per second to base::GetRandInt",
      static_cast<double>(num_rounds * kBatchSize) / (now - start).InSecondsF(),
      "number of calls per second", true);
}

// Logs the average number of calls per second to RandIntDeprecated(). Time is
// measured every 1000 calls and the total number of calls is normalized to
// reflect the average number of calls  per second.
TEST(RandUtilPerfTest, AverageNumberOfCallsPerSecondToRandIntDeprecated) {
  base::TimeTicks start = base::TimeTicks::Now();
  base::TimeTicks now;

  size_t num_rounds = 0;
  constexpr int kBatchSize = 1000;

  do {
    int num_calls = 0;
    while (num_calls < kBatchSize) {
      RandIntDeprecated(0, 1000);
      ++num_calls;
    }
    now = base::TimeTicks::Now();
    ++num_rounds;
  } while (now - start < base::TimeDelta::FromSeconds(1));

  perf_test::PrintResult(
      "Task", " (Time is measured after every 1000 function calls)",
      "Average number of calls per second to RandIntDeprecated()",
      static_cast<double>(num_rounds * kBatchSize) / (now - start).InSecondsF(),
      "number of calls per second", true);
}

}  // namespace base
