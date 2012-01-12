// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/stringprintf.h"
#include "base/test/trace_event_analyzer.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/tracing.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/perf/browser_perf_test.h"
#include "chrome/test/perf/perf_test.h"
#include "content/public/common/content_switches.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/gl/gl_switches.h"

namespace {

enum ThroughputTestFlags {
  kNone = 0,
  kInternal = 1 << 0 // Test uses internal test data.
};

const int kSpinUpTimeMs = 5 * 1000;
const int kRunTimeMs = 10 * 1000;
const int kIgnoreSomeFrames = 3;

class ThroughputTest : public BrowserPerfTest {
 public:
  explicit ThroughputTest(bool use_gpu) : use_gpu_(use_gpu) {}

  bool IsGpuAvailable() const {
    return CommandLine::ForCurrentProcess()->HasSwitch("enable-gpu");
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    BrowserPerfTest::SetUpCommandLine(command_line);
    // We are measuring throughput, so we don't want any FPS throttling.
    command_line->AppendSwitch(switches::kDisableGpuVsync);
    // Default behavior is to thumbnail the tab after 0.5 seconds, causing
    // a nasty frame hitch and disturbing the latency test. Fix that:
    command_line->AppendSwitch(switches::kEnableInBrowserThumbnailing);
    command_line->AppendSwitch(switches::kAllowFileAccessFromFiles);
    // Enable or disable GPU acceleration.
    if (use_gpu_) {
      if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableThreadedCompositing)) {
        command_line->AppendSwitch(switches::kEnableThreadedCompositing);
      }
      command_line->AppendSwitch(switches::kEnableAccelerated2dCanvas);
      command_line->AppendSwitch(switches::kForceCompositingMode);
    } else {
      command_line->AppendSwitch(switches::kDisableAcceleratedCompositing);
      command_line->AppendSwitch(switches::kDisableExperimentalWebGL);
      command_line->AppendSwitch(switches::kDisableAccelerated2dCanvas);
    }
  }

  void Wait(int ms) {
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE, MessageLoop::QuitClosure(), ms);
    ui_test_utils::RunMessageLoop();
  }

  void RunTest(const std::string& test_name) {
    RunTest(test_name, kNone);
  }

  void RunTest(const std::string& test_name, ThroughputTestFlags flags) {
    using trace_analyzer::Query;
    using trace_analyzer::TraceAnalyzer;
    using trace_analyzer::TraceEventVector;

    if (use_gpu_ && !IsGpuAvailable()) {
      LOG(WARNING) << "Test skipped: requires gpu. Pass --enable-gpu on the "
                      "command line if use of GPU is desired.\n";
      return;
    }

    // Set path to test html.
    FilePath test_path;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_path));
    test_path = test_path.Append(FILE_PATH_LITERAL("perf"));
    if (flags & kInternal)
      test_path = test_path.Append(FILE_PATH_LITERAL("private"));
    test_path = test_path.Append(FILE_PATH_LITERAL("rendering"));
    test_path = test_path.Append(FILE_PATH_LITERAL("throughput"));
    test_path = test_path.AppendASCII(test_name);
    test_path = test_path.Append(FILE_PATH_LITERAL("index.html"));
    ASSERT_TRUE(file_util::PathExists(test_path))
        << "Missing test file: " << test_path.value();

    std::string json_events;
    TraceEventVector events_sw, events_gpu;
    scoped_ptr<TraceAnalyzer> analyzer;

    ui_test_utils::NavigateToURLWithDisposition(
        browser(), net::FilePathToFileURL(test_path), CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_NONE);

    // Let the test spin up.
    LOG(INFO) << "Spinning up test...\n";
    ASSERT_TRUE(tracing::BeginTracing("test_gpu"));
    Wait(kSpinUpTimeMs);
    ASSERT_TRUE(tracing::EndTracing(&json_events));

    // Check if GPU is rendering:
    analyzer.reset(TraceAnalyzer::Create(json_events));
    bool ran_on_gpu = (analyzer->FindEvents(Query::EventName() ==
        Query::String("SwapBuffers"), &events_gpu) > 0u);
    LOG(INFO) << "Mode: " << (ran_on_gpu ? "GPU" : "Software");
    EXPECT_EQ(use_gpu_, ran_on_gpu);

    // Let the test run for a while.
    LOG(INFO) << "Running test...\n";
    ASSERT_TRUE(tracing::BeginTracing("test_fps"));
    Wait(kRunTimeMs);
    ASSERT_TRUE(tracing::EndTracing(&json_events));

    // Search for frame ticks. We look for both SW and GPU frame ticks so that
    // the test can verify that only one or the other are found.
    analyzer.reset(TraceAnalyzer::Create(json_events));
    Query query_sw = Query::EventName() == Query::String("TestFrameTickSW");
    Query query_gpu = Query::EventName() == Query::String("TestFrameTickGPU");
    analyzer->FindEvents(query_sw, &events_sw);
    analyzer->FindEvents(query_gpu, &events_gpu);
    TraceEventVector* frames = NULL;
    if (use_gpu_) {
      frames = &events_gpu;
      EXPECT_EQ(0u, events_sw.size());
    } else {
      frames = &events_sw;
      EXPECT_EQ(0u, events_gpu.size());
    }
    printf("Frame tick events: %d\n", static_cast<int>(frames->size()));
    ASSERT_GT(frames->size(), 20u);
    // Cull a few leading and trailing events as they might be unreliable.
    TraceEventVector rate_events(frames->begin() + kIgnoreSomeFrames,
                                 frames->end() - kIgnoreSomeFrames);
    trace_analyzer::RateStats stats;
    ASSERT_TRUE(GetRateStats(rate_events, &stats));
    printf("FPS = %f\n", 1000000.0 / stats.mean_us);

    // Print perf results.
    double mean_ms = stats.mean_us / 1000.0;
    double std_dev_ms = stats.standard_deviation_us / 1000.0 / 1000.0;
    std::string trace_name = ran_on_gpu ? "gpu" : "software";
    std::string mean_and_error = base::StringPrintf("%f,%f", mean_ms,
                                                    std_dev_ms);
    perf_test::PrintResultMeanAndError(test_name, "", trace_name,
                                       mean_and_error, "frame_time", true);
  }

 private:
  bool use_gpu_;
};

// For running tests on GPU:
class ThroughputTestGPU : public ThroughputTest {
 public:
  ThroughputTestGPU() : ThroughputTest(true) {}
};

// For running tests on Software:
class ThroughputTestSW : public ThroughputTest {
 public:
  ThroughputTestSW() : ThroughputTest(false) {}
};

////////////////////////////////////////////////////////////////////////////////
/// Tests

IN_PROC_BROWSER_TEST_F(ThroughputTestGPU, Particles) {
  RunTest("particles", kInternal);
}

IN_PROC_BROWSER_TEST_F(ThroughputTestSW, CanvasDemoSW) {
  RunTest("canvas-demo", kInternal);
}

IN_PROC_BROWSER_TEST_F(ThroughputTestGPU, CanvasDemoGPU) {
  RunTest("canvas-demo", kInternal);
}

}  // namespace
