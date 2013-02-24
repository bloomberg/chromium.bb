// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/test/test_timeouts.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/automation_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/perf/perf_test.h"
#include "chrome/test/ui/javascript_test_util.h"
#include "chrome/test/ui/ui_perf_test.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"

namespace {

static const base::FilePath::CharType kStartFile[] =
    FILE_PATH_LITERAL("perf_test.html");

class IndexedDBTest : public UIPerfTest {
 public:
  typedef std::map<std::string, std::string> ResultsMap;

  IndexedDBTest() : reference_(false) {
    dom_automation_enabled_ = true;
    show_window_ = true;
  }

  void RunTest() {
    base::FilePath::StringType start_file(kStartFile);
    base::FilePath test_path = GetIndexedDBTestDir();
    test_path = test_path.Append(start_file);
    GURL test_url(net::FilePathToFileURL(test_path));

    scoped_refptr<TabProxy> tab(GetActiveTab());
    ASSERT_TRUE(tab.get());
    ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS, tab->NavigateToURL(test_url));

    // Wait for the test to finish.
    ASSERT_TRUE(WaitUntilTestCompletes(tab.get(), test_url));

    PrintResults(tab.get());
  }

 protected:
  bool reference_;  // True if this is a reference build.

 private:
  // Return the path to the IndexedDB test directory on the local filesystem.
  base::FilePath GetIndexedDBTestDir() {
    base::FilePath test_dir;
    PathService::Get(chrome::DIR_TEST_DATA, &test_dir);
    return test_dir.AppendASCII("indexeddb");
  }

  bool WaitUntilTestCompletes(TabProxy* tab, const GURL& test_url) {
    return WaitUntilCookieValue(tab, test_url, "__done",
                                TestTimeouts::large_test_timeout(), "1");
  }

  bool GetResults(TabProxy* tab, ResultsMap* results) {
    std::wstring json_wide;
    bool succeeded = tab->ExecuteAndExtractString(L"",
        L"window.domAutomationController.send("
        L"    JSON.stringify(automation.getResults()));",
        &json_wide);

    EXPECT_TRUE(succeeded);
    if (!succeeded)
      return false;

    std::string json = base::WideToUTF8(json_wide);
    return JsonDictionaryToMap(json, results);
  }

  void PrintResults(TabProxy* tab) {
    ResultsMap results;
    ASSERT_TRUE(GetResults(tab, &results));

    std::string trace_name = reference_ ? "t_ref" : "t";

    ResultsMap::const_iterator it = results.begin();
    for (; it != results.end(); ++it)
      perf_test::PrintResultList(it->first, "", trace_name, it->second, "ms",
                                 false);
  }

  DISALLOW_COPY_AND_ASSIGN(IndexedDBTest);
};

class IndexedDBReferenceTest : public IndexedDBTest {
 public:
  IndexedDBReferenceTest() : IndexedDBTest() {
    reference_ = true;
  }

  virtual void SetUp() {
    UseReferenceBuild();
    IndexedDBTest::SetUp();
  }
};

TEST_F(IndexedDBTest, Perf) {

  RunTest();
}

TEST_F(IndexedDBReferenceTest, Perf) {

  RunTest();
}

}  // namespace
