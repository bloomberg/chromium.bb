// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/process_memory_metrics_emitter.h"

#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/histogram_tester.h"
#include "base/test/trace_event_analyzer.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_config_memory_test_util.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/tracing.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/test_utils.h"
#include "extensions/features/features.h"
#include "net/dns/mock_host_resolver.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/test_extension_dir.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension.h"
#include "extensions/test/background_page_watcher.h"
#endif

namespace {

using base::trace_event::MemoryDumpType;
using memory_instrumentation::mojom::ProcessType;

#if BUILDFLAG(ENABLE_EXTENSIONS)
using extensions::BackgroundPageWatcher;
using extensions::Extension;
using extensions::ProcessManager;
using extensions::TestExtensionDir;
#endif

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
                       bool check_minimum,
                       int number_of_processes = 1u) {
  std::unique_ptr<base::HistogramSamples> samples(
      histogram_tester.GetHistogramSamplesSinceCreation(name));
  ASSERT_TRUE(samples);

  bool count_matches = samples->TotalCount() == count * number_of_processes;
  // The exact number of renderers present at the time the metrics are emitted
  // is not deterministic. Sometimes there is an extra renderer.
  if (name.find("Renderer") != std::string::npos) {
    count_matches = samples->TotalCount() >= (count * number_of_processes) &&
                    samples->TotalCount() <= (number_of_processes + 1) * count;
  }

  EXPECT_TRUE(count_matches);

  if (check_minimum)
    EXPECT_GT(samples->sum(), 0u) << name;

  // As a sanity check, no memory stat should exceed 4 GB.
  int64_t maximum_expected_size = 1ll << 32;
  EXPECT_LT(samples->sum(), maximum_expected_size) << name;
}

void CheckAllMemoryMetrics(const base::HistogramTester& histogram_tester,
                           int count,
                           int number_of_renderer_processes = 1u,
                           int number_of_extenstion_processes = 0u) {
  CheckMemoryMetric("Memory.Experimental.Browser2.Malloc", histogram_tester,
                    count, true);
  CheckMemoryMetric("Memory.Experimental.Browser2.Resident", histogram_tester,
                    count, true);
  CheckMemoryMetric("Memory.Experimental.Browser2.PrivateMemoryFootprint",
                    histogram_tester, count, true);
  if (number_of_renderer_processes) {
    CheckMemoryMetric("Memory.Experimental.Renderer2.Malloc", histogram_tester,
                      count, true, number_of_renderer_processes);
    CheckMemoryMetric("Memory.Experimental.Renderer2.Resident",
                      histogram_tester, count, true,
                      number_of_renderer_processes);
    CheckMemoryMetric("Memory.Experimental.Renderer2.BlinkGC", histogram_tester,
                      count, false, number_of_renderer_processes);
    CheckMemoryMetric("Memory.Experimental.Renderer2.PartitionAlloc",
                      histogram_tester, count, false,
                      number_of_renderer_processes);
    CheckMemoryMetric("Memory.Experimental.Renderer2.V8", histogram_tester,
                      count, true, number_of_renderer_processes);
    CheckMemoryMetric("Memory.Experimental.Renderer2.PrivateMemoryFootprint",
                      histogram_tester, count, true,
                      number_of_renderer_processes);
  }
  if (number_of_extenstion_processes) {
    CheckMemoryMetric("Memory.Experimental.Extension2.Malloc", histogram_tester,
                      count, true, number_of_extenstion_processes);
    CheckMemoryMetric("Memory.Experimental.Extension2.Resident",
                      histogram_tester, count, true,
                      number_of_extenstion_processes);
    CheckMemoryMetric("Memory.Experimental.Extension2.BlinkGC",
                      histogram_tester, count, false,
                      number_of_extenstion_processes);
    CheckMemoryMetric("Memory.Experimental.Extension2.PartitionAlloc",
                      histogram_tester, count, false,
                      number_of_extenstion_processes);
    CheckMemoryMetric("Memory.Experimental.Extension2.V8", histogram_tester,
                      count, true, number_of_extenstion_processes);
    CheckMemoryMetric("Memory.Experimental.Extension2.PrivateMemoryFootprint",
                      histogram_tester, count, true,
                      number_of_extenstion_processes);
  }
  CheckMemoryMetric("Memory.Experimental.Total2.PrivateMemoryFootprint",
                    histogram_tester, count, true);
}

}  // namespace

class ProcessMemoryMetricsEmitterTest : public ExtensionBrowserTest {
 public:
  ProcessMemoryMetricsEmitterTest() {}
  ~ProcessMemoryMetricsEmitterTest() override {}
  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  void PreRunTestOnMainThread() override {
    InProcessBrowserTest::PreRunTestOnMainThread();

    // UKM DCHECKs if the active UkmRecorder is changed from one instance
    // to another, rather than being changed from a nullptr; browser_tests
    // need to circumvent that to be able to intercept UKM calls with its
    // own TestUkmRecorder instance rather than the default UkmRecorder.
    ukm::UkmRecorder::Set(nullptr);
    test_ukm_recorder_ = base::MakeUnique<ukm::TestAutoSetUkmRecorder>();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kEnableFeatures,
                                    ukm::kUkmFeature.name);
  }

 protected:
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;

  void CheckAllUkmSources(size_t metric_count = 1u) {
    const std::map<ukm::SourceId, std::unique_ptr<ukm::UkmSource>>& sources =
        test_ukm_recorder_->GetSources();
    for (auto& pair : sources) {
      const ukm::UkmSource* source = pair.second.get();
      if (ProcessHasTypeForSource(source, ProcessType::BROWSER)) {
        CheckUkmBrowserSource(source, metric_count);
      } else if (ProcessHasTypeForSource(source, ProcessType::RENDERER)) {
        CheckUkmRendererSource(source, metric_count);
      } else {
        // This must be Total2.
        CheckMemoryMetricWithName(source, "Total2.PrivateMemoryFootprint",
                                  false, metric_count);
      }
    }
  }

  void CheckMemoryMetricWithName(const ukm::UkmSource* source,
                                 const char* name,
                                 bool can_be_zero,
                                 size_t metric_count = 1u) {
    std::vector<int64_t> metrics =
        test_ukm_recorder_->GetMetrics(*source, UkmEventName, name);
    EXPECT_EQ(metric_count, metrics.size());
    EXPECT_GE(metrics[0], can_be_zero ? 0 : 1) << name;
    EXPECT_LE(metrics[0], 4000) << name;
  }

  void CheckUkmRendererSource(const ukm::UkmSource* source,
                              size_t metric_count = 1u) {
    CheckMemoryMetricWithName(source, "Malloc", false, metric_count);
    CheckMemoryMetricWithName(source, "Resident", false, metric_count);
    CheckMemoryMetricWithName(source, "PrivateMemoryFootprint", false,
                              metric_count);
    CheckMemoryMetricWithName(source, "BlinkGC", true, metric_count);
    CheckMemoryMetricWithName(source, "PartitionAlloc", true, metric_count);
    CheckMemoryMetricWithName(source, "V8", true, metric_count);
    CheckMemoryMetricWithName(source, "NumberOfExtensions", true, metric_count);
  }

  void CheckUkmBrowserSource(const ukm::UkmSource* source,
                             size_t metric_count = 1u) {
    CheckMemoryMetricWithName(source, "Malloc", false, metric_count);
    CheckMemoryMetricWithName(source, "Resident", false, metric_count);
    CheckMemoryMetricWithName(source, "PrivateMemoryFootprint", false,
                              metric_count);
  }

  bool ProcessHasTypeForSource(const ukm::UkmSource* source,
                               ProcessType process_type) {
    std::vector<int64_t> metrics =
        test_ukm_recorder_->GetMetrics(*source, UkmEventName, "ProcessType");

    return std::find(metrics.begin(), metrics.end(),
                     static_cast<int64_t>(process_type)) != metrics.end();
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Create an barebones extension with a background page for the given name.
  const Extension* CreateExtension(const std::string& name) {
    auto dir = base::MakeUnique<TestExtensionDir>();
    dir->WriteManifestWithSingleQuotes(
        base::StringPrintf("{"
                           "'name': '%s',"
                           "'version': '1',"
                           "'manifest_version': 2,"
                           "'background': {'page': 'bg.html'}"
                           "}",
                           name.c_str()));
    dir->WriteFile(FILE_PATH_LITERAL("bg.html"), "");

    const Extension* extension = LoadExtension(dir->UnpackedPath());
    EXPECT_TRUE(extension);
    temp_dirs_.push_back(std::move(dir));
    return extension;
  }

  const Extension* CreateHostedApp(const std::string& name,
                                   const GURL& app_url) {
    std::unique_ptr<TestExtensionDir> dir(new TestExtensionDir);
    dir->WriteManifestWithSingleQuotes(base::StringPrintf(
        "{"
        "'name': '%s',"
        "'version': '1',"
        "'manifest_version': 2,"
        "'app': {'urls': ['%s'], 'launch': {'web_url': '%s'}}"
        "}",
        name.c_str(), app_url.spec().c_str(), app_url.spec().c_str()));

    const Extension* extension = LoadExtension(dir->UnpackedPath());
    EXPECT_TRUE(extension);
    temp_dirs_.push_back(std::move(dir));
    return extension;
  }

#endif

 private:
#if BUILDFLAG(ENABLE_EXTENSIONS)
  std::vector<std::unique_ptr<TestExtensionDir>> temp_dirs_;
#endif
  DISALLOW_COPY_AND_ASSIGN(ProcessMemoryMetricsEmitterTest);
};

#if defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER)
#define MAYBE_FetchAndEmitMetrics DISABLED_FetchAndEmitMetrics
#else
#define MAYBE_FetchAndEmitMetrics FetchAndEmitMetrics
#endif
IN_PROC_BROWSER_TEST_F(ProcessMemoryMetricsEmitterTest,
                       MAYBE_FetchAndEmitMetrics) {
  GURL url1(url::kAboutBlankURL);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url1, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  base::HistogramTester histogram_tester;
  base::RunLoop run_loop;

  // Intentionally let emitter leave scope to check that it correctly keeps
  // itself alive.
  {
    scoped_refptr<ProcessMemoryMetricsEmitterFake> emitter(
        new ProcessMemoryMetricsEmitterFake(&run_loop,
                                            test_ukm_recorder_.get()));
    emitter->FetchAndEmitProcessMemoryMetrics();
  }

  run_loop.Run();

  CheckAllMemoryMetrics(histogram_tester, 1);
  CheckAllUkmSources();
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
#if defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER)
#define MAYBE_FetchAndEmitMetricsWithExtensions \
  DISABLED_FetchAndEmitMetricsWithExtensions
#else
#define MAYBE_FetchAndEmitMetricsWithExtensions \
  FetchAndEmitMetricsWithExtensions
#endif
IN_PROC_BROWSER_TEST_F(ProcessMemoryMetricsEmitterTest,
                       MAYBE_FetchAndEmitMetricsWithExtensions) {
  const Extension* extension1 = CreateExtension("Extension 1");
  const Extension* extension2 = CreateExtension("Extension 2");
  ProcessManager* pm = ProcessManager::Get(profile());

  // Verify that the extensions has loaded.
  BackgroundPageWatcher(pm, extension1).WaitForOpen();
  BackgroundPageWatcher(pm, extension2).WaitForOpen();
  EXPECT_EQ(1u, pm->GetRenderFrameHostsForExtension(extension1->id()).size());
  EXPECT_EQ(1u, pm->GetRenderFrameHostsForExtension(extension2->id()).size());

  GURL url1(url::kAboutBlankURL);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url1, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  base::HistogramTester histogram_tester;
  base::RunLoop run_loop;

  // Intentionally let emitter leave scope to check that it correctly keeps
  // itself alive.
  {
    scoped_refptr<ProcessMemoryMetricsEmitterFake> emitter(
        new ProcessMemoryMetricsEmitterFake(&run_loop,
                                            test_ukm_recorder_.get()));
    emitter->FetchAndEmitProcessMemoryMetrics();
  }

  run_loop.Run();

  CheckAllMemoryMetrics(histogram_tester, 1, 1, 2);
  // Extensions do not get a UKM record.
  CheckAllUkmSources();
}

#if defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER)
#define MAYBE_FetchAndEmitMetricsWithHostedApps \
  DISABLED_FetchAndEmitMetricsWithHostedApps
#else
#define MAYBE_FetchAndEmitMetricsWithHostedApps \
  FetchAndEmitMetricsWithHostedApps
#endif
IN_PROC_BROWSER_TEST_F(ProcessMemoryMetricsEmitterTest,
                       MAYBE_FetchAndEmitMetricsWithHostedApps) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url = embedded_test_server()->GetURL("app.org", "/empty.html");
  const Extension* app = CreateHostedApp("App", GURL("http://app.org"));
  ui_test_utils::NavigateToURL(browser(), app_url);

  // Verify that the hosted app has loaded.
  ProcessManager* pm = ProcessManager::Get(profile());
  EXPECT_EQ(1u, pm->GetRenderFrameHostsForExtension(app->id()).size());

  GURL url1(url::kAboutBlankURL);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url1, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  base::HistogramTester histogram_tester;
  base::RunLoop run_loop;

  // Intentionally let emitter leave scope to check that it correctly keeps
  // itself alive.
  {
    scoped_refptr<ProcessMemoryMetricsEmitterFake> emitter(
        new ProcessMemoryMetricsEmitterFake(&run_loop,
                                            test_ukm_recorder_.get()));
    emitter->FetchAndEmitProcessMemoryMetrics();
  }

  run_loop.Run();

  // No extensions should be observed
  CheckAllMemoryMetrics(histogram_tester, 1, 1, 0);
  CheckAllUkmSources();
}

#if defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER)
#define MAYBE_FetchAndEmitMetricsWithExtensionsAndHostReuse \
  DISABLED_FetchAndEmitMetricsWithExtensionsAndHostReuse
#else
#define MAYBE_FetchAndEmitMetricsWithExtensionsAndHostReuse \
  FetchAndEmitMetricsWithExtensionsAndHostReuse
#endif
IN_PROC_BROWSER_TEST_F(ProcessMemoryMetricsEmitterTest,
                       MAYBE_FetchAndEmitMetricsWithExtensionsAndHostReuse) {
  // This test does not work with --site-per-process flag since this test
  // combines multiple extensions in the same process.
  if (content::AreAllSitesIsolatedForTesting())
    return;
  // Limit the number of renderer processes to force reuse.
  content::RenderProcessHost::SetMaxRendererProcessCount(1);
  const Extension* extension1 = CreateExtension("Extension 1");
  const Extension* extension2 = CreateExtension("Extension 2");
  ProcessManager* pm = ProcessManager::Get(profile());

  // Verify that the extensions has loaded.
  BackgroundPageWatcher(pm, extension1).WaitForOpen();
  BackgroundPageWatcher(pm, extension2).WaitForOpen();
  EXPECT_EQ(1u, pm->GetRenderFrameHostsForExtension(extension1->id()).size());
  EXPECT_EQ(1u, pm->GetRenderFrameHostsForExtension(extension2->id()).size());

  GURL url1(url::kAboutBlankURL);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url1, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  base::HistogramTester histogram_tester;
  base::RunLoop run_loop;

  // Intentionally let emitter leave scope to check that it correctly keeps
  // itself alive.
  {
    scoped_refptr<ProcessMemoryMetricsEmitterFake> emitter(
        new ProcessMemoryMetricsEmitterFake(&run_loop,
                                            test_ukm_recorder_.get()));
    emitter->FetchAndEmitProcessMemoryMetrics();
  }

  run_loop.Run();

  // When hosts share a process, no unique URL is identified, therefore no UKM.
  CheckAllMemoryMetrics(histogram_tester, 1, 1, 1);
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER)
#define MAYBE_FetchDuringTrace DISABLED_FetchDuringTrace
#else
#define MAYBE_FetchDuringTrace FetchDuringTrace
#endif
IN_PROC_BROWSER_TEST_F(ProcessMemoryMetricsEmitterTest,
                       MAYBE_FetchDuringTrace) {
  GURL url1(url::kAboutBlankURL);
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
        new ProcessMemoryMetricsEmitterFake(&run_loop,
                                            test_ukm_recorder_.get()));
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
  CheckAllUkmSources();
}

#if defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER)
#define MAYBE_FetchThreeTimes DISABLED_FetchThreeTimes
#else
#define MAYBE_FetchThreeTimes FetchThreeTimes
#endif
IN_PROC_BROWSER_TEST_F(ProcessMemoryMetricsEmitterTest, MAYBE_FetchThreeTimes) {
  GURL url1(url::kAboutBlankURL);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url1, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  base::HistogramTester histogram_tester;
  base::RunLoop run_loop;

  int count = 3;
  for (int i = 0; i < count; ++i) {
    // Only the last emitter should stop the run loop.
    auto emitter = base::MakeRefCounted<ProcessMemoryMetricsEmitterFake>(
        (i == count - 1) ? &run_loop : nullptr, test_ukm_recorder_.get());
    emitter->FetchAndEmitProcessMemoryMetrics();
  }

  run_loop.Run();

  CheckAllMemoryMetrics(histogram_tester, count);
  CheckAllUkmSources(count);
}
