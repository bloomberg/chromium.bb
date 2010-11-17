// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/ref_counted.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"

// This browser test is aimed towards exercising the IndexedDB bindings and
// the actual implementation that lives in the browser side (in_process_webkit).
class IndexedDBBrowserTest : public InProcessBrowserTest {
 public:
  IndexedDBBrowserTest() {
    EnableDOMAutomation();
  }

  GURL testUrl(const FilePath& file_path) {
    const FilePath kTestDir(FILE_PATH_LITERAL("indexeddb"));
    return ui_test_utils::GetTestUrl(kTestDir, file_path);
  }

  void SimpleTest(const GURL& test_url) {
    // The test page will perform tests on IndexedDB, then navigate to either
    // a #pass or #fail ref.
    ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
        browser(), test_url, 2);
    std::string result = browser()->GetSelectedTabContents()->GetURL().ref();
    if (result != "pass") {
      std::string js_result;
      ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractString(
          browser()->GetSelectedTabContents()->render_view_host(), L"",
          L"window.domAutomationController.send(getLog())", &js_result));
      FAIL() << "Failed: " << js_result;
    }
  }
};

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, CursorTest) {
  SimpleTest(testUrl(FilePath(FILE_PATH_LITERAL("cursor_test.html"))));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, IndexTest) {
  SimpleTest(testUrl(FilePath(FILE_PATH_LITERAL("index_test.html"))));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, KeyPathTest) {
  SimpleTest(testUrl(FilePath(FILE_PATH_LITERAL("key_path_test.html"))));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, TransactionGetTest) {
  SimpleTest(testUrl(FilePath(FILE_PATH_LITERAL("transaction_get_test.html"))));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, ObjectStoreTest) {
  SimpleTest(testUrl(FilePath(FILE_PATH_LITERAL("object_store_test.html"))));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, DatabaseTest) {
  SimpleTest(testUrl(FilePath(FILE_PATH_LITERAL("database_test.html"))));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, TransactionTest) {
  SimpleTest(testUrl(FilePath(FILE_PATH_LITERAL("transaction_test.html"))));
}
