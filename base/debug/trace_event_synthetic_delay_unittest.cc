// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/trace_event_synthetic_delay.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace debug {
namespace {

const int kTargetDurationMs = 100;
// Allow some leeway in timings to make it possible to run these tests with a
// wall clock time source too.
const int kShortDurationMs = 10;

}  // namespace

class TraceEventSyntheticDelayTest : public testing::Test,
                                     public TraceEventSyntheticDelayClock {
 public:
  TraceEventSyntheticDelayTest() {}

  // TraceEventSyntheticDelayClock implementation.
  virtual base::TimeTicks Now() OVERRIDE {
    AdvanceTime(base::TimeDelta::FromMilliseconds(kShortDurationMs / 10));
    return now_;
  }

  TraceEventSyntheticDelay* ConfigureDelay(const char* name) {
    TraceEventSyntheticDelay* delay = TraceEventSyntheticDelay::Lookup(name);
    delay->SetClock(this);
    delay->SetTargetDuration(
        base::TimeDelta::FromMilliseconds(kTargetDurationMs));
    return delay;
  }

  void AdvanceTime(base::TimeDelta delta) { now_ += delta; }

  int TestFunction() {
    base::TimeTicks start = Now();
    { TRACE_EVENT_SYNTHETIC_DELAY("test.Delay"); }
    return (Now() - start).InMilliseconds();
  }

  int AsyncTestFunctionActivate() {
    base::TimeTicks start = Now();
    { TRACE_EVENT_SYNTHETIC_DELAY_ACTIVATE("test.AsyncDelay"); }
    return (Now() - start).InMilliseconds();
  }

  int AsyncTestFunctionApply() {
    base::TimeTicks start = Now();
    { TRACE_EVENT_SYNTHETIC_DELAY_APPLY("test.AsyncDelay"); }
    return (Now() - start).InMilliseconds();
  }

 private:
  base::TimeTicks now_;

  DISALLOW_COPY_AND_ASSIGN(TraceEventSyntheticDelayTest);
};

TEST_F(TraceEventSyntheticDelayTest, StaticDelay) {
  TraceEventSyntheticDelay* delay = ConfigureDelay("test.Delay");
  delay->SetMode(TraceEventSyntheticDelay::STATIC);
  EXPECT_GE(TestFunction(), kTargetDurationMs);
}

TEST_F(TraceEventSyntheticDelayTest, OneShotDelay) {
  TraceEventSyntheticDelay* delay = ConfigureDelay("test.Delay");
  delay->SetMode(TraceEventSyntheticDelay::ONE_SHOT);
  EXPECT_GE(TestFunction(), kTargetDurationMs);
  EXPECT_LT(TestFunction(), kShortDurationMs);

  delay->SetTargetDuration(
      base::TimeDelta::FromMilliseconds(kTargetDurationMs));
  EXPECT_GE(TestFunction(), kTargetDurationMs);
}

TEST_F(TraceEventSyntheticDelayTest, AlternatingDelay) {
  TraceEventSyntheticDelay* delay = ConfigureDelay("test.Delay");
  delay->SetMode(TraceEventSyntheticDelay::ALTERNATING);
  EXPECT_GE(TestFunction(), kTargetDurationMs);
  EXPECT_LT(TestFunction(), kShortDurationMs);
  EXPECT_GE(TestFunction(), kTargetDurationMs);
  EXPECT_LT(TestFunction(), kShortDurationMs);
}

TEST_F(TraceEventSyntheticDelayTest, AsyncDelay) {
  ConfigureDelay("test.AsyncDelay");
  EXPECT_LT(AsyncTestFunctionActivate(), kShortDurationMs);
  EXPECT_GE(AsyncTestFunctionApply(), kTargetDurationMs / 2);
}

TEST_F(TraceEventSyntheticDelayTest, AsyncDelayExceeded) {
  ConfigureDelay("test.AsyncDelay");
  EXPECT_LT(AsyncTestFunctionActivate(), kShortDurationMs);
  AdvanceTime(base::TimeDelta::FromMilliseconds(kTargetDurationMs));
  EXPECT_LT(AsyncTestFunctionApply(), kShortDurationMs);
}

TEST_F(TraceEventSyntheticDelayTest, AsyncDelayNoActivation) {
  ConfigureDelay("test.AsyncDelay");
  EXPECT_LT(AsyncTestFunctionApply(), kShortDurationMs);
}

TEST_F(TraceEventSyntheticDelayTest, AsyncDelayMultipleActivations) {
  ConfigureDelay("test.AsyncDelay");
  EXPECT_LT(AsyncTestFunctionActivate(), kShortDurationMs);
  AdvanceTime(base::TimeDelta::FromMilliseconds(kTargetDurationMs));
  EXPECT_LT(AsyncTestFunctionActivate(), kShortDurationMs);
  EXPECT_LT(AsyncTestFunctionApply(), kShortDurationMs);
}

}  // namespace debug
}  // namespace base
