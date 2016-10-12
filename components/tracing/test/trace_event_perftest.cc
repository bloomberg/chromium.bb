// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/run_loop.h"
#include "base/trace_event/trace_event.h"
#include "perf_test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tracing {
namespace {

using base::trace_event::TraceConfig;
using base::trace_event::TraceLog;
using base::trace_event::TraceRecordMode;

const int kNumRuns = 100;

class TraceEventPerfTest : public ::testing::Test {
 public:
  void BeginTrace() {
    TraceConfig config("*", "");
    config.SetTraceRecordMode(TraceRecordMode::RECORD_CONTINUOUSLY);
    TraceLog::GetInstance()->SetEnabled(config, TraceLog::RECORDING_MODE);
  }

  void EndTraceAndFlush() {
    ScopedStopwatch stopwatch("flush");
    base::RunLoop run_loop;
    TraceLog::GetInstance()->SetDisabled();
    TraceLog::GetInstance()->Flush(
        Bind(&OnTraceDataCollected, run_loop.QuitClosure()));
    run_loop.Run();
  }

  static void OnTraceDataCollected(
      base::Closure quit_closure,
      const scoped_refptr<base::RefCountedString>& events_str,
      bool has_more_events) {

    if (!has_more_events)
      quit_closure.Run();
  }
};

TEST_F(TraceEventPerfTest, Submit_10000_TRACE_EVENT0) {
  BeginTrace();
  IterableStopwatch stopwatch("events");
  for (int lap = 0; lap < kNumRuns; lap++) {
    for (int i = 0; i < 10000; i++) {
      TRACE_EVENT0("test_category", "TRACE_EVENT0 call");
    }
    stopwatch.NextLap();
  }
  EndTraceAndFlush();
}

}  // namespace
}  // namespace tracing
