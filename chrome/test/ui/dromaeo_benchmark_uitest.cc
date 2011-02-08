// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/test/test_timeouts.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/json_value_serializer.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/javascript_test_util.h"
#include "chrome/test/ui/ui_perf_test.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"

namespace {

const char kRunDromaeo[] = "run-dromaeo-benchmark";

class DromaeoTest : public UIPerfTest {
 public:
  typedef std::map<std::string, std::string> ResultsMap;

  DromaeoTest() : reference_(false) {
    dom_automation_enabled_ = true;
    show_window_ = true;
  }

  void RunTest(const FilePath::CharType* suite) {
    FilePath test_path = GetDromaeoDir();
    test_path = test_path.Append(
        FilePath::StringType(FILE_PATH_LITERAL("index.html?")) +
        FilePath::StringType(suite) +
        FilePath::StringType(FILE_PATH_LITERAL("&automated")));
    GURL test_url(net::FilePathToFileURL(test_path));

    scoped_refptr<TabProxy> tab(GetActiveTab());
    ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS, tab->NavigateToURL(test_url));

    // Wait for the test to finish.
    ASSERT_TRUE(WaitUntilTestCompletes(tab.get(), test_url));

    PrintResults(tab.get());
  }

 protected:
  bool reference_;  // True if this is a reference build.

 private:
  // Return the path to the dromaeo benchmark directory on the local filesystem.
  FilePath GetDromaeoDir() {
    FilePath test_dir;
    PathService::Get(chrome::DIR_TEST_DATA, &test_dir);
    return test_dir.AppendASCII("dromaeo");
  }

  bool WaitUntilTestCompletes(TabProxy* tab, const GURL& test_url) {
    return WaitUntilCookieValue(tab, test_url, "__done",
                                TestTimeouts::huge_test_timeout_ms(), "1");
  }

  bool GetScore(TabProxy* tab, std::string* score) {
    std::wstring score_wide;
    bool succeeded = tab->ExecuteAndExtractString(L"",
        L"window.domAutomationController.send(automation.GetScore());",
        &score_wide);

    // Note that we don't use ASSERT_TRUE here (and in some other places) as it
    // doesn't work inside a function with a return type other than void.
    EXPECT_TRUE(succeeded);
    if (!succeeded)
      return false;

    score->assign(WideToUTF8(score_wide));
    return true;
  }

  bool GetResults(TabProxy* tab, ResultsMap* results) {
    std::wstring json_wide;
    bool succeeded = tab->ExecuteAndExtractString(L"",
        L"window.domAutomationController.send("
        L"    JSON.stringify(automation.GetResults()));",
        &json_wide);

    EXPECT_TRUE(succeeded);
    if (!succeeded)
      return false;

    std::string json = WideToUTF8(json_wide);
    return JsonDictionaryToMap(json, results);
  }

  void PrintResults(TabProxy* tab) {
    std::string score;
    ASSERT_TRUE(GetScore(tab, &score));

    ResultsMap results;
    ASSERT_TRUE(GetResults(tab, &results));

    std::string trace_name = reference_ ? "score_ref" : "score";
    std::string unit_name = "runs/s";

    ResultsMap::const_iterator it = results.begin();
    // First result is overall score and thus "important".
    bool important = true;
    for (; it != results.end(); ++it) {
      std::string test_name = it->first;
      for (size_t i = 0; i < test_name.length(); i++)
        if (!isalnum(test_name[i]))
          test_name[i] = '_';
      PrintResult(test_name, "", trace_name, it->second, unit_name, important);
      important = false;  // All others are not overall scores.
    }
  }

  DISALLOW_COPY_AND_ASSIGN(DromaeoTest);
};

class DromaeoReferenceTest : public DromaeoTest {
 public:
  DromaeoReferenceTest() : DromaeoTest() {
    reference_ = true;
  }

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
    UITest::SetUp();
  }
};

TEST_F(DromaeoTest, DOMCorePerf) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(kRunDromaeo))
    return;

  RunTest(FILE_PATH_LITERAL("dom"));
}

TEST_F(DromaeoTest, JSLibPerf) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(kRunDromaeo))
    return;

  RunTest(FILE_PATH_LITERAL("jslib"));
}

TEST_F(DromaeoReferenceTest, DOMCorePerf) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(kRunDromaeo))
    return;

  RunTest(FILE_PATH_LITERAL("dom"));
}

TEST_F(DromaeoReferenceTest, JSLibPerf) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(kRunDromaeo))
    return;

  RunTest(FILE_PATH_LITERAL("jslib"));
}


}  // namespace
