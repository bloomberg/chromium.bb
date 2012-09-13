// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/perf/perf_test.h"
#include "chrome/test/ui/javascript_test_util.h"
#include "chrome/test/ui/ui_perf_test.h"
#include "content/public/test/browser_test_utils.h"
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

  void RunTest(const std::string& suite) {
    if (!CommandLine::ForCurrentProcess()->HasSwitch(kRunDromaeo))
      return;
    FilePath test_path = GetDromaeoDir();
    std::string query_string = suite + "&automated";
    test_path = test_path.Append(FILE_PATH_LITERAL("index.html"));
    GURL test_url(content::GetFileUrlWithQuery(test_path, query_string));

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
                                TestTimeouts::large_test_timeout(), "1");
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
      perf_test::PrintResult(test_name, "", trace_name, it->second, unit_name,
                             important);
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

  void SetUp() {
    UseReferenceBuild();
    DromaeoTest::SetUp();
  }
};

#if defined(OS_WIN)
// http://crbug.com/134570 - is flaky on Win7 perf bot
#define MAYBE_DOMCorePerf DISABLED_DOMCorePerf
#define MAYBE_DOMCoreModifyPerf DISABLED_DOMCoreModifyPerf
#else
#define MAYBE_DOMCorePerf DOMCorePerf
#define MAYBE_DOMCoreModifyPerf DOMCoreModifyPerf
#endif
TEST_F(DromaeoTest, MAYBE_DOMCorePerf) {
  RunTest("dom");
}

TEST_F(DromaeoTest, DOMCoreAttrPerf) {
  RunTest("dom-attr");
}

TEST_F(DromaeoTest, MAYBE_DOMCoreModifyPerf) {
  RunTest("dom-modify");
}

TEST_F(DromaeoTest, DOMCoreQueryPerf) {
  RunTest("dom-query");
}

TEST_F(DromaeoTest, DOMCoreTraversePerf) {
  RunTest("dom-traverse");
}

TEST_F(DromaeoTest, JSLibPerf) {
  RunTest("jslib");
}

TEST_F(DromaeoTest, JSLibAttrJqueryPerf) {
  RunTest("jslib-attr-jquery");
}

TEST_F(DromaeoTest, JSLibAttrPrototypePerf) {
  RunTest("jslib-attr-prototype");
}

TEST_F(DromaeoTest, JSLibEventJqueryPerf) {
  RunTest("jslib-event-jquery");
}

TEST_F(DromaeoTest, JSLibEventPrototypePerf) {
  RunTest("jslib-event-prototype");
}

TEST_F(DromaeoTest, JSLibModifyJqueryPerf) {
  RunTest("jslib-modify-jquery");
}

TEST_F(DromaeoTest, JSLibModifyPrototypePerf) {
  RunTest("jslib-modify-prototype");
}

TEST_F(DromaeoTest, JSLibTraverseJqueryPerf) {
  RunTest("jslib-traverse-jquery");
}

TEST_F(DromaeoTest, JSLibTraversePrototypePerf) {
  RunTest("jslib-traverse-prototype");
}

TEST_F(DromaeoTest, JSLibStyleJqueryPerf) {
  RunTest("jslib-style-jquery");
}

TEST_F(DromaeoTest, JSLibStylePrototypePerf) {
  RunTest("jslib-style-prototype");
}

TEST_F(DromaeoReferenceTest, MAYBE_DOMCorePerf) {
  RunTest("dom");
}

TEST_F(DromaeoReferenceTest, DOMCoreAttrPerf) {
  RunTest("dom-attr");
}

TEST_F(DromaeoReferenceTest, DOMCoreModifyPerf) {
  RunTest("dom-modify");
}

TEST_F(DromaeoReferenceTest, DOMCoreQueryPerf) {
  RunTest("dom-query");
}

TEST_F(DromaeoReferenceTest, DOMCoreTraversePerf) {
  RunTest("dom-traverse");
}

TEST_F(DromaeoReferenceTest, JSLibPerf) {
  RunTest("jslib");
}

TEST_F(DromaeoReferenceTest, JSLibAttrJqueryPerf) {
  RunTest("jslib-attr-jquery");
}

TEST_F(DromaeoReferenceTest, JSLibAttrPrototypePerf) {
  RunTest("jslib-attr-prototype");
}

TEST_F(DromaeoReferenceTest, JSLibEventJqueryPerf) {
  RunTest("jslib-event-jquery");
}

TEST_F(DromaeoReferenceTest, JSLibEventPrototypePerf) {
  RunTest("jslib-event-prototype");
}

TEST_F(DromaeoReferenceTest, JSLibModifyJqueryPerf) {
  RunTest("jslib-modify-jquery");
}

TEST_F(DromaeoReferenceTest, JSLibModifyPrototypePerf) {
  RunTest("jslib-modify-prototype");
}

TEST_F(DromaeoReferenceTest, JSLibTraverseJqueryPerf) {
  RunTest("jslib-traverse-jquery");
}

TEST_F(DromaeoReferenceTest, JSLibTraversePrototypePerf) {
  RunTest("jslib-traverse-prototype");
}

TEST_F(DromaeoReferenceTest, JSLibStyleJqueryPerf) {
  RunTest("jslib-style-jquery");
}

TEST_F(DromaeoReferenceTest, JSLibStylePrototypePerf) {
  RunTest("jslib-style-prototype");
}


}  // namespace
