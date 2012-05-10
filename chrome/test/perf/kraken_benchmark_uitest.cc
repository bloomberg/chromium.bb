// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/test/test_timeouts.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/perf/perf_test.h"
#include "chrome/test/ui/javascript_test_util.h"
#include "chrome/test/ui/ui_perf_test.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"

namespace {

const char kRunKraken[] = "run-kraken-benchmark";

class KrakenBenchmarkTest : public UIPerfTest {
 public:
  typedef std::map<std::string, std::string> ResultsMap;

  KrakenBenchmarkTest() : reference_(false) {
    dom_automation_enabled_ = true;
    show_window_ = true;
  }

  void RunTest() {
    FilePath test_path;
    PathService::Get(chrome::DIR_APP, &test_path);
    test_path = test_path.AppendASCII("test")
        .AppendASCII("data")
        .AppendASCII("third_party")
        .AppendASCII("kraken")
        .AppendASCII("hosted")
        .AppendASCII("kraken-1.1")
        .Append(FILE_PATH_LITERAL("driver.html"));
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
  bool WaitUntilTestCompletes(TabProxy* tab, const GURL& test_url) {
    return WaitUntilCookieValue(tab, test_url, "__done",
                                TestTimeouts::large_test_timeout_ms(), "1");
  }

  bool GetResults(TabProxy* tab, ResultsMap* results) {
    std::wstring json_wide;
    bool succeeded = tab->ExecuteAndExtractString(L"",
        L"window.domAutomationController.send("
        L"    automation.getResults());",
        &json_wide);

    EXPECT_TRUE(succeeded);
    if (!succeeded)
      return false;

    std::string json = WideToUTF8(json_wide);
    return JsonDictionaryToMap(json, results);
  }

  void PrintResults(TabProxy* tab) {
    ResultsMap results;
    ASSERT_TRUE(GetResults(tab, &results));

    std::string trace_name = reference_ ? "score_ref" : "score";
    perf_test::PrintResult("score", "", trace_name, results["score"], "ms",
                           true);

    ResultsMap::const_iterator it = results.begin();
    for (; it != results.end(); ++it) {
      if (it->first == "score")
        continue;
      perf_test::PrintResult(it->first, "", trace_name, it->second, "ms",
                             false);
    }
  }

  DISALLOW_COPY_AND_ASSIGN(KrakenBenchmarkTest);
};

class KrakenBenchmarkReferenceTest : public KrakenBenchmarkTest {
 public:
  KrakenBenchmarkReferenceTest() : KrakenBenchmarkTest() {
    reference_ = true;
  }

  void SetUp() {
    UseReferenceBuild();
    KrakenBenchmarkTest::SetUp();
  }
};

TEST_F(KrakenBenchmarkTest, Perf) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(kRunKraken))
    return;

  RunTest();
}

TEST_F(KrakenBenchmarkReferenceTest, Perf) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(kRunKraken))
    return;

  RunTest();
}

}  // namespace
