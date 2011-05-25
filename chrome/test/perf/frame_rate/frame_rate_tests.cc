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
  }

  virtual FilePath GetDataPath(const std::string& name) {
    // Make sure the test data is checked out.
    FilePath test_path;
    PathService::Get(chrome::DIR_TEST_DATA, &test_path);
    test_path = test_path.Append(FILE_PATH_LITERAL("perf"));
    test_path = test_path.Append(FILE_PATH_LITERAL("frame_rate"));
    test_path = test_path.AppendASCII(name);
    return test_path;
  }

  void RunTest(const std::string& name, const std::string& suffix) {
    FilePath test_path = GetDataPath(name);
    ASSERT_TRUE(file_util::DirectoryExists(test_path))
        << "Missing test directory: " << test_path.value();

    test_path = test_path.Append(FILE_PATH_LITERAL("test.html"));

    scoped_refptr<TabProxy> tab(GetActiveTab());
    ASSERT_TRUE(tab.get());

    ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS,
              tab->NavigateToURL(net::FilePathToFileURL(test_path)));

    // Start the test.
    ASSERT_TRUE(tab->NavigateToURLAsync(GURL("javascript:__start();")));

    // Block until the test completes.
    ASSERT_TRUE(WaitUntilJavaScriptCondition(
        tab, L"", L"window.domAutomationController.send(!__running);",
        TestTimeouts::huge_test_timeout_ms()));

    // Read out the results.
    std::wstring json;
    ASSERT_TRUE(tab->ExecuteAndExtractString(
        L"",
        L"window.domAutomationController.send("
        L"JSON.stringify(__calc_results()));",
        &json));

    std::map<std::string, std::string> results;
    ASSERT_TRUE(JsonDictionaryToMap(WideToUTF8(json), &results));

    ASSERT_TRUE(results.find("mean") != results.end());
    ASSERT_TRUE(results.find("sigma") != results.end());

    std::string mean_and_error = results["mean"] + "," + results["sigma"];
    PrintResultMeanAndError("fps" + suffix, "", "", mean_and_error,
                            "frames-per-second", false);
  }
};

class FrameRateTest_Reference : public FrameRateTest {
 public:
  // Override the browser directory that is used by UITest::SetUp to cause it
  // to use the reference build instead.
  void SetUp() {
    FilePath dir;
    PathService::Get(chrome::DIR_TEST_TOOLS, &dir);
    dir = dir.AppendASCII("reference_build");
#if defined(OS_WIN)
    dir = dir.AppendASCII("chrome");
#elif defined(OS_LINUX)
    dir = dir.AppendASCII("chrome_linux");
#elif defined(OS_MACOSX)
    dir = dir.AppendASCII("chrome_mac");
#endif
    browser_directory_ = dir;
    FrameRateTest::SetUp();
  }
};

TEST_F(FrameRateTest, Blank) {
  RunTest("blank", "");
}

// TODO(darin): Need to update the reference build to a version that supports
// the webkitRequestAnimationFrame API.
#if 0
TEST_F(FrameRateTest_Reference, Blank) {
  RunTest("blank", "_ref");
}
#endif

}  // namespace
