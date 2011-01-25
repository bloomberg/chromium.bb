// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/ref_counted.h"
#include "base/scoped_temp_dir.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/in_process_webkit/indexed_db_context.h"
#include "chrome/browser/in_process_webkit/webkit_context.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/testing_profile.h"
#include "chrome/test/thread_test_helper.h"
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
    LOG(INFO) << "Navigating to URL and blocking.";
    ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
        browser(), test_url, 2);
    LOG(INFO) << "Navigation done.";
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

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, DoesntHangTest) {
  SimpleTest(testUrl(FilePath(
      FILE_PATH_LITERAL("transaction_run_forever.html"))));
  ui_test_utils::CrashTab(browser()->GetSelectedTabContents());
  SimpleTest(testUrl(FilePath(FILE_PATH_LITERAL("transaction_test.html"))));
}

// In proc browser test is needed here because ClearLocalState indirectly calls
// WebKit's isMainThread through WebSecurityOrigin->SecurityOrigin.
IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, ClearLocalState) {
  // Create test files.
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  FilePath indexeddb_dir = temp_dir.path().Append(
      IndexedDBContext::kIndexedDBDirectory);
  ASSERT_TRUE(file_util::CreateDirectory(indexeddb_dir));

  FilePath::StringType file_name_1(FILE_PATH_LITERAL("http_foo_0"));
  file_name_1.append(IndexedDBContext::kIndexedDBExtension);
  FilePath::StringType file_name_2(FILE_PATH_LITERAL("chrome-extension_foo_0"));
  file_name_2.append(IndexedDBContext::kIndexedDBExtension);
  FilePath temp_file_path_1 = indexeddb_dir.Append(file_name_1);
  FilePath temp_file_path_2 = indexeddb_dir.Append(file_name_2);

  ASSERT_EQ(1, file_util::WriteFile(temp_file_path_1, ".", 1));
  ASSERT_EQ(1, file_util::WriteFile(temp_file_path_2, "o", 1));

  // Create the scope which will ensure we run the destructor of the webkit
  // context which should trigger the clean up.
  {
    TestingProfile profile;
    WebKitContext *webkit_context = profile.GetWebKitContext();
    webkit_context->indexed_db_context()->set_data_path(indexeddb_dir);
    webkit_context->set_clear_local_state_on_exit(true);
  }
  // Make sure we wait until the destructor has run.
  scoped_refptr<ThreadTestHelper> helper(
      new ThreadTestHelper(BrowserThread::WEBKIT));
  ASSERT_TRUE(helper->Run());

  // Because we specified https for scheme to be skipped the second file
  // should survive and the first go into vanity.
  ASSERT_FALSE(file_util::PathExists(temp_file_path_1));
  ASSERT_TRUE(file_util::PathExists(temp_file_path_2));
}
