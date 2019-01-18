// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "base/test/trace_event_analyzer.h"
#include "chrome/browser/extensions/api/tab_capture/tab_capture_performance_test_base.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_launcher_utils.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/tracing.h"
#include "content/public/common/content_switches.h"
#include "extensions/common/switches.h"
#include "extensions/test/extension_test_message_listener.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_test.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/gl/gl_switches.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace {

// Number of events to trim from the begining and end. These events don't
// contribute anything toward stable measurements: A brief moment of startup
// "jank" is acceptable, and shutdown may result in missing events (since
// render widget draws may stop before capture stops).
constexpr size_t kTrimEvents = 24;  // 1 sec at 24fps, or 0.4 sec at 60 fps.

// Minimum number of events required for a reasonable analysis.
constexpr size_t kMinDataPoints = 100;  // ~5 sec at 24fps.

enum TestFlags {
  kUseGpu = 1 << 0,              // Only execute test if --enable-gpu was given
                                 // on the command line.  This is required for
                                 // tests that run on GPU.
  kTestThroughWebRTC = 1 << 3,   // Send video through a webrtc loopback.
  kSmallWindow = 1 << 4,         // Window size: 1 = 800x600, 0 = 2000x1000
};

class TabCapturePerformanceTest : public TabCapturePerformanceTestBase,
                                  public testing::WithParamInterface<int> {
 public:
  TabCapturePerformanceTest() = default;
  ~TabCapturePerformanceTest() override = default;

  bool HasFlag(TestFlags flag) const {
    return (GetParam() & flag) == flag;
  }

  std::string GetSuffixForTestFlags() const {
    std::string suffix;
    if (HasFlag(kUseGpu))
      suffix += "_comp_gpu";
    if (HasFlag(kTestThroughWebRTC))
      suffix += "_webrtc";
    if (HasFlag(kSmallWindow))
      suffix += "_small";
    return suffix;
  }

  void SetUp() override {
    const base::FilePath test_file = GetApiTestDataDir()
                                         .AppendASCII("tab_capture")
                                         .AppendASCII("balls.html");
    const bool success = base::ReadFileToString(test_file, &test_page_html_);
    CHECK(success) << "Failed to load test page at: "
                   << test_file.AsUTF8Unsafe();

    if (!HasFlag(kUseGpu))
      UseSoftwareCompositing();

    TabCapturePerformanceTestBase::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (HasFlag(kSmallWindow)) {
      command_line->AppendSwitchASCII(switches::kWindowSize, "800,600");
    } else {
      command_line->AppendSwitchASCII(switches::kWindowSize, "2000,1500");
    }

    TabCapturePerformanceTestBase::SetUpCommandLine(command_line);
  }

  static void GetTraceEvents(trace_analyzer::TraceAnalyzer* analyzer,
                             const std::string& event_name,
                             trace_analyzer::TraceEventVector* events) {
    trace_analyzer::Query query =
        trace_analyzer::Query::EventNameIs(event_name) &&
        (trace_analyzer::Query::EventPhaseIs(TRACE_EVENT_PHASE_BEGIN) ||
         trace_analyzer::Query::EventPhaseIs(TRACE_EVENT_PHASE_ASYNC_BEGIN) ||
         trace_analyzer::Query::EventPhaseIs(TRACE_EVENT_PHASE_FLOW_BEGIN) ||
         trace_analyzer::Query::EventPhaseIs(TRACE_EVENT_PHASE_INSTANT) ||
         trace_analyzer::Query::EventPhaseIs(TRACE_EVENT_PHASE_COMPLETE));
    analyzer->FindEvents(query, events);
    VLOG(0) << "Retrieved " << events->size() << " events for: " << event_name;
    ASSERT_LT(2 * kTrimEvents + kMinDataPoints, events->size())
        << "Not enough events of type " << event_name << " found for analysis.";
  }

  // Analyze and print the mean and stddev of how often events having the name
  // |event_name| occur.
  bool PrintRateResults(trace_analyzer::TraceAnalyzer* analyzer,
                        const std::string& event_name) {
    trace_analyzer::TraceEventVector events;
    GetTraceEvents(analyzer, event_name, &events);

    // Ignore some events for startup/setup/caching/teardown.
    trace_analyzer::TraceEventVector rate_events(events.begin() + kTrimEvents,
                                                 events.end() - kTrimEvents);
    trace_analyzer::RateStats stats;
    if (!GetRateStats(rate_events, &stats, NULL)) {
      LOG(ERROR) << "GetRateStats failed";
      return false;
    }
    double mean_ms = stats.mean_us / 1000.0;
    double std_dev_ms = stats.standard_deviation_us / 1000.0;
    std::string mean_and_error = base::StringPrintf("%f,%f", mean_ms,
                                                    std_dev_ms);
    perf_test::PrintResultMeanAndError(kTestName, GetSuffixForTestFlags(),
                                       event_name, mean_and_error, "ms", true);
    return true;
  }

  // Analyze and print the mean and stddev of the amount of time between the
  // begin and end timestamps of each event having the name |event_name|.
  bool PrintLatencyResults(trace_analyzer::TraceAnalyzer* analyzer,
                           const std::string& event_name) {
    trace_analyzer::TraceEventVector events;
    GetTraceEvents(analyzer, event_name, &events);

    // Ignore some events for startup/setup/caching/teardown.
    trace_analyzer::TraceEventVector events_to_analyze(
        events.begin() + kTrimEvents, events.end() - kTrimEvents);

    // Compute mean and standard deviation of all capture latencies.
    double sum = 0.0;
    double sqr_sum = 0.0;
    int count = 0;
    for (const auto* begin_event : events_to_analyze) {
      const auto* end_event = begin_event->other_event;
      if (!end_event)
        continue;
      const double latency = end_event->timestamp - begin_event->timestamp;
      sum += latency;
      sqr_sum += latency * latency;
      ++count;
    }
    DCHECK_GE(static_cast<size_t>(count), kMinDataPoints);
    const double mean_us = sum / count;
    const double std_dev_us =
        sqrt(std::max(0.0, count * sqr_sum - sum * sum)) / count;
    perf_test::PrintResultMeanAndError(
        kTestName, GetSuffixForTestFlags(), event_name + "Latency",
        base::StringPrintf("%f,%f", mean_us / 1000.0, std_dev_us / 1000.0),
        "ms", true);
    return true;
  }

  // Analyze and print the mean and stddev of how often events having the name
  // |event_name| are missing the success=true flag.
  bool PrintFailRateResults(trace_analyzer::TraceAnalyzer* analyzer,
                            const std::string& event_name) {
    trace_analyzer::TraceEventVector events;
    GetTraceEvents(analyzer, event_name, &events);

    // Ignore some events for startup/setup/caching/teardown.
    trace_analyzer::TraceEventVector events_to_analyze(
        events.begin() + kTrimEvents, events.end() - kTrimEvents);

    // Compute percentage of begin→end events missing a success=true flag.
    double fail_percent = 100.0;
    if (events_to_analyze.empty()) {
      // If there are no events to analyze, then the failure rate is 100%.
    } else {
      int fail_count = 0;
      for (const auto* begin_event : events_to_analyze) {
        const auto* end_event = begin_event->other_event;
        if (!end_event) {
          // This indicates the operation never completed, and so is counted as
          // a failure.
          ++fail_count;
          continue;
        }
        const auto it = end_event->arg_numbers.find("success");
        if (it == end_event->arg_numbers.end()) {
          LOG(ERROR) << "Missing 'success' value in Capture end event.";
          return false;
        }
        if (it->second == 0.0) {
          ++fail_count;
        }
      }
      fail_percent *= fail_count / events_to_analyze.size();
    }
    perf_test::PrintResult(
        kTestName, GetSuffixForTestFlags(), event_name + "FailRate",
        base::StringPrintf("%f", fail_percent), "percent", true);
    return true;
  }

 protected:
  // The HTML test web page that draws animating balls continuously. Populated
  // in SetUp().
  std::string test_page_html_;

  // Naming of performance measurement written to stdout.
  static const char kTestName[];
};

// static
const char TabCapturePerformanceTest::kTestName[] = "TabCapturePerformance";

}  // namespace

IN_PROC_BROWSER_TEST_P(TabCapturePerformanceTest, Performance) {
  // Load the extension and test page, and tell the extension to start tab
  // capture.
  LoadExtension(GetApiTestDataDir()
                    .AppendASCII("tab_capture")
                    .AppendASCII("perftest_extension"));
  NavigateToTestPage(test_page_html_);
  const base::Value response = SendMessageToExtension(
      base::StringPrintf("{start:true, passThroughWebRTC:%s}",
                         HasFlag(kTestThroughWebRTC) ? "true" : "false"));
  const std::string* reason = response.FindStringKey("reason");
  ASSERT_TRUE(response.FindBoolKey("success").value_or(false))
      << (reason ? *reason : std::string("<MISSING REASON>"));

  // Observe the running browser for a while, collecting a trace.
  const std::string json_events = TraceAndObserve("gpu,gpu.capture");

  std::unique_ptr<trace_analyzer::TraceAnalyzer> analyzer;
  analyzer.reset(trace_analyzer::TraceAnalyzer::Create(json_events));
  analyzer->AssociateAsyncBeginEndEvents();

  // The printed result will be the average time between composites in the
  // renderer of the page being captured. This may not reach the full frame
  // rate if the renderer cannot draw as fast as is desired.
  //
  // Note that any changes to drawing or compositing in the renderer,
  // including changes to Blink (e.g., Canvas drawing), layout, etc.; will
  // have an impact on this result.
  EXPECT_TRUE(PrintRateResults(
      analyzer.get(), "RenderWidget::DidCommitAndDrawCompositorFrame"));

  // This prints out the average time between capture events in the browser
  // process. This should roughly match the renderer's draw+composite rate.
  EXPECT_TRUE(PrintRateResults(analyzer.get(), "Capture"));

  // Analyze mean/stddev of the capture latency. This is a measure of how long
  // each capture took, from initiation until read-back from the GPU into a
  // media::VideoFrame was complete. Lower is better.
  EXPECT_TRUE(PrintLatencyResults(analyzer.get(), "Capture"));

  // Analyze percentage of failed captures. This measures how often captures
  // were initiated, but not completed successfully. Lower is better, and zero
  // is ideal.
  EXPECT_TRUE(PrintFailRateResults(analyzer.get(), "Capture"));
}

// Note: First argument is optional and intentionally left blank.
// (it's a prefix for the generated test cases)
INSTANTIATE_TEST_CASE_P(,
                        TabCapturePerformanceTest,
                        testing::Values(0,
                                        kUseGpu,
                                        kTestThroughWebRTC,
                                        kTestThroughWebRTC | kUseGpu));
