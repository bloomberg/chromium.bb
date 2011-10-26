// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_number_conversions.h"
#include "base/test/test_timeouts.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/javascript_test_util.h"
#include "chrome/test/ui/ui_perf_test.h"
#include "net/base/net_util.h"
#include "ui/gfx/gl/gl_switches.h"

namespace {

enum FrameRateTestFlags {
  kMakeBodyComposited = 1 << 0,
  kDisableVsync       = 1 << 1,
  kDisableGpu         = 1 << 2,
  kUseReferenceBuild  = 1 << 3,
  kInternal           = 1 << 4,
};

std::string GetSuffixForTestFlags(int flags) {
  std::string suffix;
  if (flags & kMakeBodyComposited)
    suffix += "_comp";
  if (flags & kDisableVsync)
    suffix += "_novsync";
  if (flags & kDisableGpu)
    suffix += "_nogpu";
  if (flags & kUseReferenceBuild)
    suffix += "_ref";
  return suffix;
}

class FrameRateTest
  : public UIPerfTest
  , public ::testing::WithParamInterface<int> {
 public:
  FrameRateTest() {
    show_window_ = true;
    dom_automation_enabled_ = true;
    // Since this is a performance test, try to use the host machine's GPU
    // instead of falling back to software-rendering.
    force_use_osmesa_ = false;
    disable_accelerated_compositing_ = false;
  }

  virtual FilePath GetDataPath(const std::string& name) {
    // Make sure the test data is checked out.
    FilePath test_path;
    PathService::Get(chrome::DIR_TEST_DATA, &test_path);
    test_path = test_path.Append(FILE_PATH_LITERAL("perf"));
    test_path = test_path.Append(FILE_PATH_LITERAL("frame_rate"));
    if (GetParam() & kInternal) {
      test_path = test_path.Append(FILE_PATH_LITERAL("private"));
    } else {
      test_path = test_path.Append(FILE_PATH_LITERAL("content"));
    }
    test_path = test_path.AppendASCII(name);
    return test_path;
  }

  virtual void SetUp() {
    if (GetParam() & kUseReferenceBuild) {
      UseReferenceBuild();
    }

    // UI tests boot up render views starting from about:blank. This causes
    // the renderer to start up thinking it cannot use the GPU. To work
    // around that, and allow the frame rate test to use the GPU, we must
    // pass kAllowWebUICompositing.
    launch_arguments_.AppendSwitch(switches::kAllowWebUICompositing);

    if (GetParam() & kDisableGpu) {
      launch_arguments_.AppendSwitch(switches::kDisableAcceleratedCompositing);
      launch_arguments_.AppendSwitch(switches::kDisableExperimentalWebGL);
    }

    if (GetParam() & kDisableVsync) {
      launch_arguments_.AppendSwitch(switches::kDisableGpuVsync);
    }

    UIPerfTest::SetUp();
  }

  void RunTest(const std::string& name) {
    FilePath test_path = GetDataPath(name);
    ASSERT_TRUE(file_util::DirectoryExists(test_path))
        << "Missing test directory: " << test_path.value();

    test_path = test_path.Append(FILE_PATH_LITERAL("test.html"));

    scoped_refptr<TabProxy> tab(GetActiveTab());
    ASSERT_TRUE(tab.get());

    ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS,
              tab->NavigateToURL(net::FilePathToFileURL(test_path)));

    if (GetParam() & kMakeBodyComposited) {
      ASSERT_TRUE(tab->NavigateToURLAsync(
        GURL("javascript:__make_body_composited();")));
    }

    // Block until initialization completes.
    ASSERT_TRUE(WaitUntilJavaScriptCondition(
        tab, L"", L"window.domAutomationController.send(__initialized);",
        TestTimeouts::large_test_timeout_ms()));

    // Start the tests.
    ASSERT_TRUE(tab->NavigateToURLAsync(GURL("javascript:__start_all();")));

    // Block until the tests completes.
    ASSERT_TRUE(WaitUntilJavaScriptCondition(
        tab, L"", L"window.domAutomationController.send(!__running_all);",
        TestTimeouts::large_test_timeout_ms()));

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

    std::string trace_name = "fps";
    trace_name += GetSuffixForTestFlags(GetParam());
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
                        kMakeBodyComposited,
                        kUseReferenceBuild,
                        kUseReferenceBuild | kMakeBodyComposited));

FRAME_RATE_TEST_WITH_AND_WITHOUT_ACCELERATED_COMPOSITING(blank);
FRAME_RATE_TEST_WITH_AND_WITHOUT_ACCELERATED_COMPOSITING(googleblog);

typedef FrameRateTest FrameRateCanvasInternalTest;

// Tests for animated 2D canvas content
#define INTERNAL_FRAME_RATE_TEST_CANVAS(content) \
TEST_P(FrameRateCanvasInternalTest, content) { \
  RunTest(#content); \
} \

INSTANTIATE_TEST_CASE_P(, FrameRateCanvasInternalTest, ::testing::Values(
                        kInternal,
                        kInternal | kDisableGpu,
                        kInternal | kUseReferenceBuild));

INTERNAL_FRAME_RATE_TEST_CANVAS(fireflies)
INTERNAL_FRAME_RATE_TEST_CANVAS(FishIE)

typedef FrameRateTest FrameRateNoVsyncCanvasInternalTest;

// Tests for animated 2D canvas content with and without disabling vsync
#define INTERNAL_FRAME_RATE_TEST_CANVAS_WITH_AND_WITHOUT_NOVSYNC(content) \
TEST_P(FrameRateNoVsyncCanvasInternalTest, content) { \
  RunTest(#content); \
} \

INSTANTIATE_TEST_CASE_P(, FrameRateNoVsyncCanvasInternalTest, ::testing::Values(
                        kInternal,
                        kInternal | kDisableVsync,
                        kInternal | kDisableGpu,
                        kInternal | kUseReferenceBuild,
                        kInternal | kDisableVsync | kUseReferenceBuild));

INTERNAL_FRAME_RATE_TEST_CANVAS_WITH_AND_WITHOUT_NOVSYNC(fishbowl)
INTERNAL_FRAME_RATE_TEST_CANVAS_WITH_AND_WITHOUT_NOVSYNC(speedreading)

}  // namespace
