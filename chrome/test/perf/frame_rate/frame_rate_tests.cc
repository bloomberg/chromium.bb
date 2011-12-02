// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_number_conversions.h"
#include "base/test/test_timeouts.h"
#include "base/test/trace_event_analyzer.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/automation/automation_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/javascript_test_util.h"
#include "chrome/test/ui/ui_perf_test.h"
#include "net/base/net_util.h"
#include "ui/gfx/gl/gl_implementation.h"
#include "ui/gfx/gl/gl_switches.h"

namespace {

enum FrameRateTestFlags {
  kUseGpu             = 1 << 0, // Only execute test if --enable-gpu, and verify
                                // that test ran on GPU. This is required for
                                // tests that run on GPU.
  kForceGpuComposited = 1 << 1, // Force the test to use the compositor.
  kDisableVsync       = 1 << 2, // Do not limit framerate to vertical refresh.
                                // when on GPU, nor to 60hz when not on GPU.
  kUseReferenceBuild  = 1 << 3, // Run test using the reference chrome build.
  kInternal           = 1 << 4, // Test uses internal test data.
  kHasRedirect        = 1 << 5, // Test page contains an HTML redirect.
};

class FrameRateTest
  : public UIPerfTest
  , public ::testing::WithParamInterface<int> {
 public:
  FrameRateTest() {
    show_window_ = true;
    dom_automation_enabled_ = true;
  }

  bool HasFlag(FrameRateTestFlags flag) const {
    return (GetParam() & flag) == flag;
  }

  bool IsGpuAvailable() const {
    return CommandLine::ForCurrentProcess()->HasSwitch("enable-gpu");
  }

  std::string GetSuffixForTestFlags() {
    std::string suffix;
    if (HasFlag(kForceGpuComposited))
      suffix += "_comp";
    if (HasFlag(kUseGpu))
      suffix += "_gpu";
    if (HasFlag(kDisableVsync))
      suffix += "_novsync";
    if (HasFlag(kUseReferenceBuild))
      suffix += "_ref";
    return suffix;
  }

  virtual FilePath GetDataPath(const std::string& name) {
    // Make sure the test data is checked out.
    FilePath test_path;
    PathService::Get(chrome::DIR_TEST_DATA, &test_path);
    test_path = test_path.Append(FILE_PATH_LITERAL("perf"));
    test_path = test_path.Append(FILE_PATH_LITERAL("frame_rate"));
    if (HasFlag(kInternal)) {
      test_path = test_path.Append(FILE_PATH_LITERAL("private"));
    } else {
      test_path = test_path.Append(FILE_PATH_LITERAL("content"));
    }
    test_path = test_path.AppendASCII(name);
    return test_path;
  }

  virtual void SetUp() {
    if (HasFlag(kUseReferenceBuild))
      UseReferenceBuild();

    // Turn on chrome.Interval to get higher-resolution timestamps on frames.
    launch_arguments_.AppendSwitch(switches::kEnableBenchmarking);

    // Required additional argument to make the kEnableBenchmarking switch work.
    launch_arguments_.AppendSwitch(switches::kEnableStatsTable);

    // UI tests boot up render views starting from about:blank. This causes
    // the renderer to start up thinking it cannot use the GPU. To work
    // around that, and allow the frame rate test to use the GPU, we must
    // pass kAllowWebUICompositing.
    launch_arguments_.AppendSwitch(switches::kAllowWebUICompositing);

    // Some of the tests may launch http requests through JSON or AJAX
    // which causes a security error (cross domain request) when the page
    // is loaded from the local file system ( file:// ). The following switch
    // fixes that error.
    launch_arguments_.AppendSwitch(switches::kAllowFileAccessFromFiles);

    if (!HasFlag(kUseGpu)) {
      launch_arguments_.AppendSwitch(switches::kDisableAcceleratedCompositing);
      launch_arguments_.AppendSwitch(switches::kDisableExperimentalWebGL);
      launch_arguments_.AppendSwitch(switches::kDisableAccelerated2dCanvas);
    } else {
      // This switch is required for enabling the accelerated 2d canvas on
      // Chrome versions prior to Chrome 15, which may be the case for the
      // reference build.
      launch_arguments_.AppendSwitch(switches::kEnableAccelerated2dCanvas);
    }

    if (HasFlag(kDisableVsync))
      launch_arguments_.AppendSwitch(switches::kDisableGpuVsync);

    UIPerfTest::SetUp();
  }

  bool DidRunOnGpu(const std::string& json_events) {
    using namespace trace_analyzer;

    // Check trace for GPU accleration.
    scoped_ptr<TraceAnalyzer> analyzer(TraceAnalyzer::Create(json_events));

    gfx::GLImplementation gl_impl = gfx::kGLImplementationNone;
    const TraceEvent* gpu_event = analyzer->FindOneEvent(
        Query(EVENT_NAME) == Query::String("SwapBuffers") &&
        Query(EVENT_HAS_NUMBER_ARG, "GLImpl"));
    if (gpu_event)
      gl_impl = static_cast<gfx::GLImplementation>(
          gpu_event->GetKnownArgAsInt("GLImpl"));
    return (gl_impl == gfx::kGLImplementationDesktopGL ||
            gl_impl == gfx::kGLImplementationEGLGLES2);
  }

  void RunTest(const std::string& name) {
    if (HasFlag(kUseGpu) && !IsGpuAvailable()) {
      printf("Test skipped: requires gpu. Pass --enable-gpu on the command "
             "line if use of GPU is desired.\n");
      return;
    }

    // Verify flag combinations.
    ASSERT_TRUE(HasFlag(kUseGpu) || !HasFlag(kForceGpuComposited));
    ASSERT_TRUE(!HasFlag(kUseGpu) || IsGpuAvailable());

    FilePath test_path = GetDataPath(name);
    ASSERT_TRUE(file_util::DirectoryExists(test_path))
        << "Missing test directory: " << test_path.value();

    test_path = test_path.Append(FILE_PATH_LITERAL("test.html"));

    scoped_refptr<TabProxy> tab(GetActiveTab());
    ASSERT_TRUE(tab.get());

    // TODO(jbates): remove this check when ref builds are updated.
    if (!HasFlag(kUseReferenceBuild))
      ASSERT_TRUE(automation()->BeginTracing("test_gpu"));

    if (HasFlag(kHasRedirect)) {
      // If the test file is known to contain an html redirect, we must block
      // until the second navigation is complete and reacquire the active tab
      // in order to avoid a race condition.
      // If the following assertion is triggered due to a timeout, it is
      // possible that the current test does not re-direct and therefore should
      // not have the kHasRedirect flag turned on.
      ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS,
        tab->NavigateToURLBlockUntilNavigationsComplete(
        net::FilePathToFileURL(test_path), 2));
      tab = GetActiveTab();
      ASSERT_TRUE(tab.get());
    } else {
      ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS,
        tab->NavigateToURL(net::FilePathToFileURL(test_path)));
    }

    // Block until initialization completes
    // If the following assertion fails intermittently, it could be due to a
    // race condition caused by an html redirect. If that is the case, verify
    // that flag kHasRedirect is enabled for the current test.
    ASSERT_TRUE(WaitUntilJavaScriptCondition(
      tab, L"", L"window.domAutomationController.send(__initialized);",
      TestTimeouts::large_test_timeout_ms()));

    if (HasFlag(kForceGpuComposited)) {
      ASSERT_TRUE(tab->NavigateToURLAsync(
        GURL("javascript:__make_body_composited();")));
    }

    // Start the tests.
    ASSERT_TRUE(tab->NavigateToURLAsync(GURL("javascript:__start_all();")));

    // Block until the tests completes.
    ASSERT_TRUE(WaitUntilJavaScriptCondition(
        tab, L"", L"window.domAutomationController.send(!__running_all);",
        TestTimeouts::large_test_timeout_ms()));

    // TODO(jbates): remove this check when ref builds are updated.
    if (!HasFlag(kUseReferenceBuild)) {
      std::string json_events;
      ASSERT_TRUE(automation()->EndTracing(&json_events));

      bool did_run_on_gpu = DidRunOnGpu(json_events);
      bool expect_gpu = HasFlag(kUseGpu);
      EXPECT_EQ(expect_gpu, did_run_on_gpu);
    }

    // Read out the results.
    std::wstring json;
    ASSERT_TRUE(tab->ExecuteAndExtractString(
        L"",
        L"window.domAutomationController.send("
        L"JSON.stringify(__calc_results_total()));",
        &json));

    std::map<std::string, std::string> results;
    ASSERT_TRUE(JsonDictionaryToMap(WideToUTF8(json), &results));

    ASSERT_TRUE(results.find("mean") != results.end());
    ASSERT_TRUE(results.find("sigma") != results.end());
    ASSERT_TRUE(results.find("gestures") != results.end());
    ASSERT_TRUE(results.find("means") != results.end());
    ASSERT_TRUE(results.find("sigmas") != results.end());

    std::string trace_name = "interval" + GetSuffixForTestFlags();
    printf("GESTURES %s: %s= [%s] [%s] [%s]\n", name.c_str(),
                                                trace_name.c_str(),
                                                results["gestures"].c_str(),
                                                results["means"].c_str(),
                                                results["sigmas"].c_str());

    std::string mean_and_error = results["mean"] + "," + results["sigma"];
    PrintResultMeanAndError(name, "", trace_name, mean_and_error,
                            "frames-per-second", true);
  }
};

// Must use a different class name to avoid test instantiation conflicts
// with FrameRateTest. An alias is good enough. The alias names must match
// the pattern FrameRate*Test* for them to get picked up by the test bots.
typedef FrameRateTest FrameRateCompositingTest;

// Tests that trigger compositing with a -webkit-translateZ(0)
#define FRAME_RATE_TEST_WITH_AND_WITHOUT_ACCELERATED_COMPOSITING(content) \
TEST_P(FrameRateCompositingTest, content) { \
  RunTest(#content); \
}

INSTANTIATE_TEST_CASE_P(, FrameRateCompositingTest, ::testing::Values(
                        0,
                        kUseGpu | kForceGpuComposited,
                        kUseReferenceBuild,
                        kUseReferenceBuild | kUseGpu | kForceGpuComposited));

FRAME_RATE_TEST_WITH_AND_WITHOUT_ACCELERATED_COMPOSITING(blank);
FRAME_RATE_TEST_WITH_AND_WITHOUT_ACCELERATED_COMPOSITING(googleblog);

typedef FrameRateTest FrameRateNoVsyncCanvasInternalTest;

// Tests for animated 2D canvas content with and without disabling vsync
#define INTERNAL_FRAME_RATE_TEST_CANVAS_WITH_AND_WITHOUT_NOVSYNC(content) \
TEST_P(FrameRateNoVsyncCanvasInternalTest, content) { \
  RunTest(#content); \
}

INSTANTIATE_TEST_CASE_P(, FrameRateNoVsyncCanvasInternalTest, ::testing::Values(
    kInternal | kHasRedirect,
    kInternal | kHasRedirect | kUseGpu,
    kInternal | kHasRedirect | kUseGpu | kDisableVsync,
    kUseReferenceBuild | kInternal | kHasRedirect,
    kUseReferenceBuild | kInternal | kHasRedirect | kUseGpu,
    kUseReferenceBuild | kInternal | kHasRedirect | kUseGpu | kDisableVsync));

INTERNAL_FRAME_RATE_TEST_CANVAS_WITH_AND_WITHOUT_NOVSYNC(fishbowl)

typedef FrameRateTest FrameRateGpuCanvasInternalTest;

// Tests for animated 2D canvas content to be tested only with GPU
// acceleration.
// tests are run with and without Vsync
#define INTERNAL_FRAME_RATE_TEST_CANVAS_GPU(content) \
TEST_P(FrameRateGpuCanvasInternalTest, content) { \
  RunTest(#content); \
}

INSTANTIATE_TEST_CASE_P(, FrameRateGpuCanvasInternalTest, ::testing::Values(
    kInternal | kHasRedirect | kUseGpu,
    kInternal | kHasRedirect | kUseGpu | kDisableVsync,
    kUseReferenceBuild | kInternal | kHasRedirect | kUseGpu,
    kUseReferenceBuild | kInternal | kHasRedirect | kUseGpu | kDisableVsync));

INTERNAL_FRAME_RATE_TEST_CANVAS_GPU(fireflies)
INTERNAL_FRAME_RATE_TEST_CANVAS_GPU(FishIE)
INTERNAL_FRAME_RATE_TEST_CANVAS_GPU(speedreading)

}  // namespace
