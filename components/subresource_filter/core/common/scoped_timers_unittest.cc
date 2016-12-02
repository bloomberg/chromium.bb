// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/common/scoped_timers.h"

#include "base/test/histogram_tester.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ExportFunctorForTests {
 public:
  int number_of_calls() const { return number_of_calls_; }
  void operator()(base::TimeDelta) { ++number_of_calls_; }

 private:
  int number_of_calls_ = 0;
};

// Casts an l-value parameter to a reference to it.
template <typename T>
T& reference(T& value) {
  return value;
}

}  // namespace

namespace subresource_filter {

TEST(ScopedTimersTest, ScopedTimerCallsFunctor) {
  ExportFunctorForTests export_functor;
  {
    // Reference is needed to bypass object copying.
    SCOPED_TIMER(reference(export_functor));
    EXPECT_EQ(0, export_functor.number_of_calls());
  }
  EXPECT_EQ(1, export_functor.number_of_calls());
}

TEST(ScopedTimersTest, ScopedThreadTimerCallsFunctor) {
  ExportFunctorForTests export_functor;
  {
    // Reference is needed to bypass functor copying.
    SCOPED_THREAD_TIMER(reference(export_functor));
    EXPECT_EQ(0, export_functor.number_of_calls());
  }
  const int expected_number_of_calls = base::ThreadTicks::IsSupported() ? 1 : 0;
  EXPECT_EQ(expected_number_of_calls, export_functor.number_of_calls());
}

TEST(ScopedTimersTest, ScopedTimersCopyFunctor) {
  ExportFunctorForTests export_functor;
  {
    SCOPED_TIMER(export_functor);
    SCOPED_THREAD_TIMER(export_functor);
    EXPECT_EQ(0, export_functor.number_of_calls());
  }
  EXPECT_EQ(0, export_functor.number_of_calls());
}

TEST(ScopedTimersTest, ScopedThreadTimerCallsStoredLambdaFunctor) {
  bool export_is_called = false;
  auto export_functor = [&export_is_called](base::TimeDelta) {
    EXPECT_FALSE(export_is_called);
    export_is_called = true;
  };

  {
    SCOPED_THREAD_TIMER(export_functor);
    EXPECT_FALSE(export_is_called);
  }
  EXPECT_EQ(base::ThreadTicks::IsSupported(), export_is_called);
}

TEST(ScopedTimersTest, ScopedTimerCallsStoredLambdaFunctor) {
  bool export_is_called = false;
  auto export_functor = [&export_is_called](base::TimeDelta) {
    export_is_called = true;
  };

  {
    SCOPED_TIMER(export_functor);
    EXPECT_FALSE(export_is_called);
  }
  EXPECT_TRUE(export_is_called);
}

TEST(ScopedTimersTest, ScopedUmaHistogramMacros) {
  base::HistogramTester tester;
  {
    SCOPED_UMA_HISTOGRAM_THREAD_TIMER("ScopedTimers.ThreadTimer");
    SCOPED_UMA_HISTOGRAM_MICRO_THREAD_TIMER("ScopedTimers.MicroThreadTimer");
    SCOPED_UMA_HISTOGRAM_MICRO_TIMER("ScopedTimers.MicroTimer");

    tester.ExpectTotalCount("ScopedTimers.ThreadTimer", 0);
    tester.ExpectTotalCount("ScopedTimers.MicroThreadTimer", 0);
    tester.ExpectTotalCount("ScopedTimers.MicroTimer", 0);
  }

  const int expected_count = base::ThreadTicks::IsSupported() ? 1 : 0;
  tester.ExpectTotalCount("ScopedTimers.ThreadTimer", expected_count);
  tester.ExpectTotalCount("ScopedTimers.MicroThreadTimer", expected_count);

  tester.ExpectTotalCount("ScopedTimers.MicroTimer", 1);
}

TEST(ScopedTimersTest, UmaHistogramMicroTimesFromExportFunctor) {
  base::HistogramTester tester;
  auto export_functor = [](base::TimeDelta delta) {
    UMA_HISTOGRAM_MICRO_TIMES("ScopedTimers.MicroTimes", delta);
  };
  {
    SCOPED_TIMER(export_functor);
    tester.ExpectTotalCount("ScopedTimers.MicroTimes", 0);
  }
  tester.ExpectTotalCount("ScopedTimers.MicroTimes", 1);
}

}  // namespace subresource_filter
