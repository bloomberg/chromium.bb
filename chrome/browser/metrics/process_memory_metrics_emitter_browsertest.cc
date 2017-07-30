// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/process_memory_metrics_emitter.h"

#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/test/histogram_tester.h"
#include "base/test/trace_event_analyzer.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_config_memory_test_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/tracing.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/test_utils.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"
#include "url/gurl.h"

namespace {

using base::trace_event::MemoryDumpType;

void RequestGlobalDumpCallback(base::Closure quit_closure,
                               bool success,
                               uint64_t) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, quit_closure);
  ASSERT_TRUE(success);
}

void OnStartTracingDoneCallback(
    base::trace_event::MemoryDumpLevelOfDetail explicit_dump_type,
    base::Closure quit_closure) {
  memory_instrumentation::MemoryInstrumentation::GetInstance()
      ->RequestGlobalDumpAndAppendToTrace(
          MemoryDumpType::EXPLICITLY_TRIGGERED, explicit_dump_type,
          Bind(&RequestGlobalDumpCallback, quit_closure));
}

class ProcessMemoryMetricsEmitterFake : public ProcessMemoryMetricsEmitter {
 public:
  explicit ProcessMemoryMetricsEmitterFake(base::RunLoop* run_loop)
      : run_loop_(run_loop) {}

 private:
  ~ProcessMemoryMetricsEmitterFake() override {}

  void ReceivedMemoryDump(
      bool success,
      uint64_t dump_guid,
      memory_instrumentation::mojom::GlobalMemoryDumpPtr ptr) override {
    EXPECT_TRUE(success);
    ProcessMemoryMetricsEmitter::ReceivedMemoryDump(success, dump_guid,
                                                    std::move(ptr));
    if (run_loop_)
      run_loop_->Quit();
  }

  base::RunLoop* run_loop_;

  DISALLOW_COPY_AND_ASSIGN(ProcessMemoryMetricsEmitterFake);
};

void CheckMemoryMetric(const std::string& name,
                       const base::HistogramTester& histogram_tester,
                       int count,
                       bool check_minimum) {
  std::unique_ptr<base::HistogramSamples> samples(
      histogram_tester.GetHistogramSamplesSinceCreation(name));
  ASSERT_TRUE(samples);

  bool count_matches = samples->TotalCount() == count;
  // The exact number of renderers present at the time the metrics are emitted
  // is not deterministic. Sometimes there are 2 renderers instead of 1.
  if (name.find("Renderer") != std::string::npos) {
    count_matches =
        samples->TotalCount() >= count && samples->TotalCount() <= 2 * count;
  }

  EXPECT_TRUE(count_matches);
  if (check_minimum)
    EXPECT_GT(samples->sum(), 0u) << name;

  // As a sanity check, no memory stat should exceed 1 GB.
  int64_t maximum_expected_size = 1ll << 30;
  EXPECT_LT(samples->sum(), maximum_expected_size) << name;
}

void CheckAllMemoryMetrics(const base::HistogramTester& histogram_tester,
                           int count) {
  CheckMemoryMetric("Memory.Experimental.Browser2.Malloc", histogram_tester,
                    count, true);
  CheckMemoryMetric("Memory.Experimental.Browser2.Resident", histogram_tester,
                    count, true);
  CheckMemoryMetric("Memory.Experimental.Browser2.PrivateMemoryFootprint",
                    histogram_tester, count, true);
  CheckMemoryMetric("Memory.Experimental.Renderer2.Malloc", histogram_tester,
                    count, true);
  CheckMemoryMetric("Memory.Experimental.Renderer2.Resident", histogram_tester,
                    count, true);
  CheckMemoryMetric("Memory.Experimental.Renderer2.BlinkGC", histogram_tester,
                    count, false);
  CheckMemoryMetric("Memory.Experimental.Renderer2.PartitionAlloc",
                    histogram_tester, count, false);
  CheckMemoryMetric("Memory.Experimental.Renderer2.V8", histogram_tester, count,
                    true);
  CheckMemoryMetric("Memory.Experimental.Renderer2.PrivateMemoryFootprint",
                    histogram_tester, count, true);
  CheckMemoryMetric("Memory.Experimental.Total2.PrivateMemoryFootprint",
                    histogram_tester, count, true);
}

}  // namespace

class ProcessMemoryMetricsEmitterTest : public InProcessBrowserTest {
 public:
  ProcessMemoryMetricsEmitterTest() {}
  ~ProcessMemoryMetricsEmitterTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ProcessMemoryMetricsEmitterTest);
};

#if defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER)
#define MAYBE_FetchAndEmitMetrics DISABLED_FetchAndEmitMetrics
#else
#define MAYBE_FetchAndEmitMetrics FetchAndEmitMetrics
#endif
IN_PROC_BROWSER_TEST_F(ProcessMemoryMetricsEmitterTest,
                       MAYBE_FetchAndEmitMetrics) {
  GURL url1("about:blank");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url1, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  base::HistogramTester histogram_tester;
  base::RunLoop run_loop;

  // Intentionally let emitter leave scope to check that it correctly keeps
  // itself alive.
  {
    scoped_refptr<ProcessMemoryMetricsEmitterFake> emitter(
        new ProcessMemoryMetricsEmitterFake(&run_loop));
    emitter->FetchAndEmitProcessMemoryMetrics();
  }

  run_loop.Run();

  CheckAllMemoryMetrics(histogram_tester, 1);
}

#if defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER)
#define MAYBE_FetchDuringTrace DISABLED_FetchDuringTrace
#else
#define MAYBE_FetchDuringTrace FetchDuringTrace
#endif
IN_PROC_BROWSER_TEST_F(ProcessMemoryMetricsEmitterTest,
                       MAYBE_FetchDuringTrace) {
  GURL url1("about:blank");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url1, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  base::HistogramTester histogram_tester;

  {
    base::RunLoop run_loop;

    base::trace_event::TraceConfig trace_config(
        base::trace_event::TraceConfigMemoryTestUtil::
            GetTraceConfig_EmptyTriggers());
    ASSERT_TRUE(tracing::BeginTracingWithTraceConfig(
        trace_config, Bind(&OnStartTracingDoneCallback,
                           base::trace_event::MemoryDumpLevelOfDetail::DETAILED,
                           run_loop.QuitClosure())));
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    scoped_refptr<ProcessMemoryMetricsEmitterFake> emitter(
        new ProcessMemoryMetricsEmitterFake(&run_loop));
    emitter->FetchAndEmitProcessMemoryMetrics();

    run_loop.Run();
  }

  std::string json_events;
  ASSERT_TRUE(tracing::EndTracing(&json_events));

  trace_analyzer::TraceEventVector events;
  std::unique_ptr<trace_analyzer::TraceAnalyzer> analyzer(
      trace_analyzer::TraceAnalyzer::Create(json_events));
  analyzer->FindEvents(
      trace_analyzer::Query::EventPhaseIs(TRACE_EVENT_PHASE_MEMORY_DUMP),
      &events);

  ASSERT_GT(events.size(), 1u);
  ASSERT_TRUE(trace_analyzer::CountMatches(
      events, trace_analyzer::Query::EventNameIs(MemoryDumpTypeToString(
                  MemoryDumpType::EXPLICITLY_TRIGGERED))));

  CheckAllMemoryMetrics(histogram_tester, 1);
}

#if defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER)
#define MAYBE_FetchThreeTimes DISABLED_FetchThreeTimes
#else
#define MAYBE_FetchThreeTimes FetchThreeTimes
#endif
IN_PROC_BROWSER_TEST_F(ProcessMemoryMetricsEmitterTest, MAYBE_FetchThreeTimes) {
  GURL url1("about:blank");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url1, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  base::HistogramTester histogram_tester;
  base::RunLoop run_loop;

  int count = 3;
  for (int i = 0; i < count; ++i) {
    // Only the last emitter should stop the run loop.
    auto emitter = base::MakeRefCounted<ProcessMemoryMetricsEmitterFake>(
        (i == count - 1) ? &run_loop : nullptr);
    emitter->FetchAndEmitProcessMemoryMetrics();
  }

  run_loop.Run();

  CheckAllMemoryMetrics(histogram_tester, count);
}
