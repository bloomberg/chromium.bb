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

namespace {

class FrameRateTest : public UIPerfTest {
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
    test_path = test_path.Append(FILE_PATH_LITERAL("content"));
    test_path = test_path.AppendASCII(name);
    return test_path;
  }

  virtual void SetUp() {
    // UI tests boot up render views starting from about:blank. This causes the
    // renderer to start up thinking it cannot use the GPU. To work around that,
    // and allow the frame rate test to use the GPU, we must pass
    // kAllowWebUICompositing.
    launch_arguments_.AppendSwitch(switches::kAllowWebUICompositing);

    UIPerfTest::SetUp();
  }

  void RunTest(const std::string& name,
               const std::string& suffix,
               bool make_body_composited) {
    FilePath test_path = GetDataPath(name);
    ASSERT_TRUE(file_util::DirectoryExists(test_path))
        << "Missing test directory: " << test_path.value();

    test_path = test_path.Append(FILE_PATH_LITERAL("test.html"));

    scoped_refptr<TabProxy> tab(GetActiveTab());
    ASSERT_TRUE(tab.get());

    ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS,
              tab->NavigateToURL(net::FilePathToFileURL(test_path)));

    if (make_body_composited) {
      ASSERT_TRUE(tab->NavigateToURLAsync(
          GURL("javascript:__make_body_composited();")));
    }

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

    std::string trace_name = "fps" + suffix;
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

class FrameRateTest_Reference : public FrameRateTest {
 public:
  void SetUp() {
    UseReferenceBuild();
    FrameRateTest::SetUp();
  }
};

#define FRAME_RATE_TEST(content) \
TEST_F(FrameRateTest, content) { \
  RunTest(#content, "", false); \
} \
TEST_F(FrameRateTest_Reference, content) { \
  RunTest(#content, "_ref", false); \
}


// Tests that trigger compositing with a -webkit-translateZ(0)
#define FRAME_RATE_TEST_WITH_AND_WITHOUT_ACCELERATED_COMPOSITING(content) \
TEST_F(FrameRateTest, content) { \
  RunTest(#content, "", false); \
} \
TEST_F(FrameRateTest, content ## _comp) { \
  RunTest(#content, "_comp", true); \
} \
TEST_F(FrameRateTest_Reference, content) { \
  RunTest(#content, "_ref", false); \
} \
TEST_F(FrameRateTest_Reference, content ## _comp) { \
  RunTest(#content, "_comp_ref", true); \
}

FRAME_RATE_TEST_WITH_AND_WITHOUT_ACCELERATED_COMPOSITING(blank);
FRAME_RATE_TEST_WITH_AND_WITHOUT_ACCELERATED_COMPOSITING(googleblog);

}  // namespace
