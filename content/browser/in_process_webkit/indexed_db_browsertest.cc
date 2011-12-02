// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/scoped_temp_dir.h"
#include "base/test/thread_test_helper.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/in_process_webkit/indexed_db_context.h"
#include "content/browser/in_process_webkit/webkit_context.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/common/content_switches.h"
#include "webkit/database/database_util.h"
#include "webkit/quota/mock_special_storage_policy.h"
#include "webkit/quota/quota_manager.h"
#include "webkit/quota/special_storage_policy.h"

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

  GURL testUrl(const FilePath& file_path) {
    const FilePath kTestDir(FILE_PATH_LITERAL("indexeddb"));
    return ui_test_utils::GetTestUrl(kTestDir, file_path);
  }

  void SimpleTest(const GURL& test_url, bool incognito = false) {
    // The test page will perform tests on IndexedDB, then navigate to either
    // a #pass or #fail ref.
    Browser* the_browser = incognito ? CreateIncognitoBrowser() : browser();

    LOG(INFO) << "Navigating to URL and blocking.";
    ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
        the_browser, test_url, 2);
    LOG(INFO) << "Navigation done.";
    std::string result = the_browser->GetSelectedTabContents()->GetURL().ref();
    if (result != "pass") {
      std::string js_result;
      ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractString(
          the_browser->GetSelectedTabContents()->render_view_host(), L"",
          L"window.domAutomationController.send(getLog())", &js_result));
      FAIL() << "Failed: " << js_result;
    }
  }
};

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, CursorTest) {
  SimpleTest(testUrl(FilePath(FILE_PATH_LITERAL("cursor_test.html"))));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, CursorTestIncognito) {
  SimpleTest(testUrl(FilePath(FILE_PATH_LITERAL("cursor_test.html"))),
             true /* incognito */);
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, CursorPrefetch) {
  SimpleTest(testUrl(FilePath(FILE_PATH_LITERAL("cursor_prefetch.html"))));
}

// Flaky: http://crbug.com/70773
IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, DISABLED_IndexTest) {
  SimpleTest(testUrl(FilePath(FILE_PATH_LITERAL("index_test.html"))));
}

// Flaky: http://crbug.com/70773
IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, DISABLED_KeyPathTest) {
  SimpleTest(testUrl(FilePath(FILE_PATH_LITERAL("key_path_test.html"))));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, TransactionGetTest) {
  SimpleTest(testUrl(FilePath(FILE_PATH_LITERAL("transaction_get_test.html"))));
}

#if defined(OS_WIN)
// http://crbug.com/104306
#define KeyTypesTest FLAKY_KeyTypesTest
#endif
IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, KeyTypesTest) {
  SimpleTest(testUrl(FilePath(FILE_PATH_LITERAL("key_types_test.html"))));
}

// Flaky: http://crbug.com/70773
IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, DISABLED_ObjectStoreTest) {
  SimpleTest(testUrl(FilePath(FILE_PATH_LITERAL("object_store_test.html"))));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, DatabaseTest) {
  SimpleTest(testUrl(FilePath(FILE_PATH_LITERAL("database_test.html"))));
}

// Flaky: http://crbug.com/70773
IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, DISABLED_TransactionTest) {
  SimpleTest(testUrl(FilePath(FILE_PATH_LITERAL("transaction_test.html"))));
}

// Flaky: http://crbug.com/70773
IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, DISABLED_DoesntHangTest) {
  SimpleTest(testUrl(FilePath(
      FILE_PATH_LITERAL("transaction_run_forever.html"))));
  ui_test_utils::CrashTab(browser()->GetSelectedTabContents());
  SimpleTest(testUrl(FilePath(FILE_PATH_LITERAL("transaction_test.html"))));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, Bug84933Test) {
  const GURL url = testUrl(FilePath(FILE_PATH_LITERAL("bug_84933.html")));

  // Just navigate to the URL. Test will crash if it fails.
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(), url, 1);
}

// In proc browser test is needed here because ClearLocalState indirectly calls
// WebKit's isMainThread through WebSecurityOrigin->SecurityOrigin.
IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, ClearLocalState) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  FilePath protected_path;
  FilePath unprotected_path;

  // Create the scope which will ensure we run the destructor of the webkit
  // context which should trigger the clean up.
  {
    TestingProfile profile;

    // Test our assumptions about what is protected and what is not.
    const GURL kProtectedOrigin("chrome-extension://foo/");
    const GURL kUnprotectedOrigin("http://foo/");
    quota::SpecialStoragePolicy* policy = profile.GetSpecialStoragePolicy();
    ASSERT_TRUE(policy->IsStorageProtected(kProtectedOrigin));
    ASSERT_FALSE(policy->IsStorageProtected(kUnprotectedOrigin));

    // Create some indexedDB paths.
    // With the levelDB backend, these are directories.
    WebKitContext *webkit_context = profile.GetWebKitContext();
    IndexedDBContext* idb_context = webkit_context->indexed_db_context();
    idb_context->set_data_path_for_testing(temp_dir.path());
    protected_path = idb_context->GetIndexedDBFilePath(
        DatabaseUtil::GetOriginIdentifier(kProtectedOrigin));
    unprotected_path = idb_context->GetIndexedDBFilePath(
        DatabaseUtil::GetOriginIdentifier(kUnprotectedOrigin));
    ASSERT_TRUE(file_util::CreateDirectory(protected_path));
    ASSERT_TRUE(file_util::CreateDirectory(unprotected_path));

    // Setup to clear all unprotected origins on exit.
    webkit_context->set_clear_local_state_on_exit(true);
  }

  // Make sure we wait until the destructor has run.
  scoped_refptr<base::ThreadTestHelper> helper(
      new base::ThreadTestHelper(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::WEBKIT)));
  ASSERT_TRUE(helper->Run());

  ASSERT_TRUE(file_util::DirectoryExists(protected_path));
  ASSERT_FALSE(file_util::DirectoryExists(unprotected_path));
}

// In proc browser test is needed here because ClearLocalState indirectly calls
// WebKit's isMainThread through WebSecurityOrigin->SecurityOrigin.
IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, ClearSessionOnlyDatabases) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  FilePath normal_path;
  FilePath session_only_path;

  // Create the scope which will ensure we run the destructor of the webkit
  // context which should trigger the clean up.
  {
    TestingProfile profile;

    const GURL kNormalOrigin("http://normal/");
    const GURL kSessionOnlyOrigin("http://session-only/");
    scoped_refptr<quota::MockSpecialStoragePolicy> special_storage_policy =
        new quota::MockSpecialStoragePolicy;
    special_storage_policy->AddSessionOnly(kSessionOnlyOrigin);

    // Create some indexedDB paths.
    // With the levelDB backend, these are directories.
    WebKitContext *webkit_context = profile.GetWebKitContext();
    IndexedDBContext* idb_context = webkit_context->indexed_db_context();

    // Override the storage policy with our own.
    idb_context->special_storage_policy_ = special_storage_policy;
    idb_context->set_data_path_for_testing(temp_dir.path());

    normal_path = idb_context->GetIndexedDBFilePath(
        DatabaseUtil::GetOriginIdentifier(kNormalOrigin));
    session_only_path = idb_context->GetIndexedDBFilePath(
        DatabaseUtil::GetOriginIdentifier(kSessionOnlyOrigin));
    ASSERT_TRUE(file_util::CreateDirectory(normal_path));
    ASSERT_TRUE(file_util::CreateDirectory(session_only_path));
  }

  // Make sure we wait until the destructor has run.
  scoped_refptr<base::ThreadTestHelper> helper(
      new base::ThreadTestHelper(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::WEBKIT)));
  ASSERT_TRUE(helper->Run());

  EXPECT_TRUE(file_util::DirectoryExists(normal_path));
  EXPECT_FALSE(file_util::DirectoryExists(session_only_path));
}

class IndexedDBBrowserTestWithLowQuota : public IndexedDBBrowserTest {
 public:
  virtual void SetUpOnMainThread() {
    const int kInitialQuotaKilobytes = 5000;
    const int kTemporaryStorageQuotaMaxSize = kInitialQuotaKilobytes
        * 1024 * QuotaManager::kPerHostTemporaryPortion;
    SetTempQuota(
        kTemporaryStorageQuotaMaxSize, browser()->profile()->GetQuotaManager());
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

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTestWithLowQuota, FAILS_QuotaTest) {
  SimpleTest(testUrl(FilePath(FILE_PATH_LITERAL("quota_test.html"))));
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
      testUrl(FilePath(FILE_PATH_LITERAL("database_callbacks_first.html"))));
}
