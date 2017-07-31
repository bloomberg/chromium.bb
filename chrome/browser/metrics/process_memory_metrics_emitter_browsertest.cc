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
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/test_utils.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"
#include "url/gurl.h"

namespace {

using base::trace_event::MemoryDumpType;
using memory_instrumentation::mojom::ProcessType;

static const char UkmEventName[] = "Memory.Experimental";

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
  explicit ProcessMemoryMetricsEmitterFake(base::RunLoop* run_loop,
                                           ukm::TestUkmRecorder* recorder)
      : run_loop_(run_loop), recorder_(recorder) {}

 private:
  ~ProcessMemoryMetricsEmitterFake() override {}

  void ReceivedMemoryDump(
      bool success,
      uint64_t dump_guid,
      memory_instrumentation::mojom::GlobalMemoryDumpPtr ptr) override {
    EXPECT_TRUE(success);
    ProcessMemoryMetricsEmitter::ReceivedMemoryDump(success, dump_guid,
                                                    std::move(ptr));
    finished_memory_dump_ = true;
    QuitIfFinished();
  }

  void ReceivedProcessInfos(
      std::vector<resource_coordinator::mojom::ProcessInfoPtr> process_infos)
      override {
    ProcessMemoryMetricsEmitter::ReceivedProcessInfos(std::move(process_infos));
    finished_process_info_ = true;
    QuitIfFinished();
  }

  void QuitIfFinished() {
    if (!finished_memory_dump_ || !finished_process_info_)
      return;
    if (run_loop_)
      run_loop_->Quit();
  }

  ukm::UkmRecorder* GetUkmRecorder() override { return recorder_; }

  base::RunLoop* run_loop_;
  bool finished_memory_dump_ = false;
  bool finished_process_info_ = false;
  ukm::TestUkmRecorder* recorder_;

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

  // As a sanity check, no memory stat should exceed 4 GB.
  int64_t maximum_expected_size = 1ll << 32;
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

 protected:
  ukm::TestUkmRecorder test_ukm_recorder_;

  void CheckUkmSourcesWithUrl(const GURL& url, size_t count) {
    std::vector<const ukm::UkmSource*> sources =
        test_ukm_recorder_.GetSourcesForUrl(url.spec().c_str());

    // There should be at least |count|, and not more than 2 * |count| renderers
    // with this URL.
    EXPECT_GE(sources.size(), count) << "Expected at least " << count
                                     << " renderers with url: " << url.spec();
    EXPECT_LE(sources.size(), 2 * count);

    // Each one should have renderer type.
    for (const ukm::UkmSource* source : sources) {
      ProcessType process_type = ProcessType::OTHER;
      ASSERT_TRUE(GetProcessTypeForSource(source, &process_type));
      EXPECT_EQ(ProcessType::RENDERER, process_type);
    }
  }

  void CheckAllUkmSources() {
    const std::map<ukm::SourceId, std::unique_ptr<ukm::UkmSource>>& sources =
        test_ukm_recorder_.GetSources();
    for (auto& pair : sources) {
      const ukm::UkmSource* source = pair.second.get();
      ProcessType process_type = ProcessType::OTHER;
      bool has_process_type = GetProcessTypeForSource(source, &process_type);

      // This must be Total2.
      if (!has_process_type) {
        CheckMemoryMetricWithName(source, "Total2.PrivateMemoryFootprint",
                                  false);
        continue;
      }

      switch (process_type) {
        case ProcessType::BROWSER:
          CheckUkmBrowserSource(source);
          break;
        case ProcessType::RENDERER:
          CheckUkmRendererSource(source);
          break;
        default:
          break;
      }
    }
  }

  void CheckMemoryMetricWithName(const ukm::UkmSource* source,
                                 const char* name,
                                 bool can_be_zero) {
    std::vector<int64_t> metrics =
        test_ukm_recorder_.GetMetrics(*source, UkmEventName, name);
    ASSERT_EQ(1u, metrics.size());
    EXPECT_GE(metrics[0], can_be_zero ? 0 : 1) << name;
    EXPECT_LE(metrics[0], 4000) << name;
  }

  void CheckUkmRendererSource(const ukm::UkmSource* source) {
    CheckMemoryMetricWithName(source, "Malloc", false);
    CheckMemoryMetricWithName(source, "Resident", false);
    CheckMemoryMetricWithName(source, "PrivateMemoryFootprint", false);
    CheckMemoryMetricWithName(source, "BlinkGC", true);
    CheckMemoryMetricWithName(source, "PartitionAlloc", true);
    CheckMemoryMetricWithName(source, "V8", true);
  }

  void CheckUkmBrowserSource(const ukm::UkmSource* source) {
    CheckMemoryMetricWithName(source, "Malloc", false);
    CheckMemoryMetricWithName(source, "Resident", false);
    CheckMemoryMetricWithName(source, "PrivateMemoryFootprint", false);
  }

  bool GetProcessTypeForSource(const ukm::UkmSource* source,
                               ProcessType* process_type) {
    std::vector<int64_t> metrics =
        test_ukm_recorder_.GetMetrics(*source, UkmEventName, "ProcessType");

    // Can't use GTEST assertion because function returns |bool|.
    if (metrics.size() >= 2u)
      ADD_FAILURE();

    if (metrics.size() == 0u)
      return false;
    *process_type = static_cast<ProcessType>(metrics[0]);
    return true;
  }

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
        new ProcessMemoryMetricsEmitterFake(&run_loop, &test_ukm_recorder_));
    emitter->FetchAndEmitProcessMemoryMetrics();
  }

  run_loop.Run();

  CheckAllMemoryMetrics(histogram_tester, 1);
  CheckUkmSourcesWithUrl(url1, 1);
  CheckAllUkmSources();
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
        new ProcessMemoryMetricsEmitterFake(&run_loop, &test_ukm_recorder_));
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
  CheckUkmSourcesWithUrl(url1, 1);
  CheckAllUkmSources();
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
        (i == count - 1) ? &run_loop : nullptr, &test_ukm_recorder_);
    emitter->FetchAndEmitProcessMemoryMetrics();
  }

  run_loop.Run();

  CheckAllMemoryMetrics(histogram_tester, count);
  CheckUkmSourcesWithUrl(url1, 3);
  CheckAllUkmSources();
}
