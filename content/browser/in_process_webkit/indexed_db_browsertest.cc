// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/test/thread_test_helper.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/in_process_webkit/indexed_db_context_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "net/base/net_util.h"
#include "webkit/database/database_util.h"
#include "webkit/quota/mock_special_storage_policy.h"
#include "webkit/quota/quota_manager.h"
#include "webkit/quota/special_storage_policy.h"

using content::BrowserContext;
using content::BrowserThread;
using quota::QuotaManager;
using webkit_database::DatabaseUtil;

// This browser test is aimed towards exercising the IndexedDB bindings and
// the actual implementation that lives in the browser side (in_process_webkit).
class IndexedDBBrowserTest : public InProcessBrowserTest {
 public:
  IndexedDBBrowserTest() {
    EnableDOMAutomation();
  }

  GURL GetTestURL(const FilePath& file_path) {
    FilePath dir;
    PathService::Get(content::DIR_TEST_DATA, &dir);
    return net::FilePathToFileURL(
        dir.Append(FILE_PATH_LITERAL("indexeddb")).Append(file_path));
  }

  void SimpleTest(const GURL& test_url, bool incognito = false) {
    // The test page will perform tests on IndexedDB, then navigate to either
    // a #pass or #fail ref.
    Browser* the_browser = incognito ? CreateIncognitoBrowser() : browser();

    LOG(INFO) << "Navigating to URL and blocking.";
    ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
        the_browser, test_url, 2);
    LOG(INFO) << "Navigation done.";
    std::string result = the_browser->GetSelectedWebContents()->GetURL().ref();
    if (result != "pass") {
      std::string js_result;
      ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractString(
          the_browser->GetSelectedWebContents()->GetRenderViewHost(), L"",
          L"window.domAutomationController.send(getLog())", &js_result));
      FAIL() << "Failed: " << js_result;
    }
  }
};

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, CursorTest) {
  SimpleTest(GetTestURL(FilePath(FILE_PATH_LITERAL("cursor_test.html"))));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, CursorTestIncognito) {
  SimpleTest(GetTestURL(FilePath(FILE_PATH_LITERAL("cursor_test.html"))),
             true /* incognito */);
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, CursorPrefetch) {
  SimpleTest(GetTestURL(FilePath(FILE_PATH_LITERAL("cursor_prefetch.html"))));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, IndexTest) {
  SimpleTest(GetTestURL(FilePath(FILE_PATH_LITERAL("index_test.html"))));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, KeyPathTest) {
  SimpleTest(GetTestURL(FilePath(FILE_PATH_LITERAL("key_path_test.html"))));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, TransactionGetTest) {
  SimpleTest(GetTestURL(FilePath(
      FILE_PATH_LITERAL("transaction_get_test.html"))));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, KeyTypesTest) {
  SimpleTest(GetTestURL(FilePath(FILE_PATH_LITERAL("key_types_test.html"))));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, ObjectStoreTest) {
  SimpleTest(GetTestURL(FilePath(FILE_PATH_LITERAL("object_store_test.html"))));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, DatabaseTest) {
  SimpleTest(GetTestURL(FilePath(FILE_PATH_LITERAL("database_test.html"))));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, TransactionTest) {
  SimpleTest(GetTestURL(FilePath(FILE_PATH_LITERAL("transaction_test.html"))));
}

// Appears flaky/slow, see: http://crbug.com/120298
IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, DISABLED_ValueSizeTest) {
  SimpleTest(GetTestURL(FilePath(FILE_PATH_LITERAL("value_size_test.html"))));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, DoesntHangTest) {
  SimpleTest(GetTestURL(FilePath(
      FILE_PATH_LITERAL("transaction_run_forever.html"))));
  ui_test_utils::CrashTab(browser()->GetSelectedWebContents());
  SimpleTest(GetTestURL(FilePath(FILE_PATH_LITERAL("transaction_test.html"))));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, Bug84933Test) {
  const GURL url = GetTestURL(FilePath(FILE_PATH_LITERAL("bug_84933.html")));

  // Just navigate to the URL. Test will crash if it fails.
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(), url, 1);
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, Bug106883Test) {
  const GURL url = GetTestURL(FilePath(FILE_PATH_LITERAL("bug_106883.html")));

  // Just navigate to the URL. Test will crash if it fails.
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(), url, 1);
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, Bug109187Test) {
  const GURL url = GetTestURL(FilePath(FILE_PATH_LITERAL("bug_109187.html")));

  // Just navigate to the URL. Test will crash if it fails.
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(), url, 1);
}

class IndexedDBBrowserTestWithLowQuota : public IndexedDBBrowserTest {
 public:
  virtual void SetUpOnMainThread() {
    const int kInitialQuotaKilobytes = 5000;
    const int kTemporaryStorageQuotaMaxSize = kInitialQuotaKilobytes
        * 1024 * QuotaManager::kPerHostTemporaryPortion;
    SetTempQuota(
        kTemporaryStorageQuotaMaxSize,
        content::BrowserContext::GetQuotaManager(browser()->profile()));
  }

  static void SetTempQuota(int64 bytes, scoped_refptr<QuotaManager> qm) {
    if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          base::Bind(&IndexedDBBrowserTestWithLowQuota::SetTempQuota, bytes,
                     qm));
      return;
    }
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    qm->SetTemporaryGlobalOverrideQuota(bytes, quota::QuotaCallback());
    // Don't return until the quota has been set.
    scoped_refptr<base::ThreadTestHelper> helper(
        new base::ThreadTestHelper(
            BrowserThread::GetMessageLoopProxyForThread(BrowserThread::DB)));
    ASSERT_TRUE(helper->Run());
  }

};

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTestWithLowQuota, QuotaTest) {
  SimpleTest(GetTestURL(FilePath(FILE_PATH_LITERAL("quota_test.html"))));
}

class IndexedDBBrowserTestWithGCExposed : public IndexedDBBrowserTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) {
    command_line->AppendSwitchASCII(switches::kJavaScriptFlags, "--expose-gc");
  }
};

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTestWithGCExposed,
                       DatabaseCallbacksTest) {
  SimpleTest(
      GetTestURL(FilePath(FILE_PATH_LITERAL("database_callbacks_first.html"))));
}
