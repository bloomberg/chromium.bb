// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/test/thread_test_helper.h"
#include "base/utf_string_conversions.h"
#include "content/browser/in_process_webkit/indexed_db_context_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/shell/shell.h"
#include "content/test/content_browser_test.h"
#include "content/test/content_browser_test_utils.h"
#include "webkit/database/database_util.h"
#include "webkit/quota/quota_manager.h"

using quota::QuotaManager;
using webkit_database::DatabaseUtil;

namespace content {

// This browser test is aimed towards exercising the IndexedDB bindings and
// the actual implementation that lives in the browser side (in_process_webkit).
class IndexedDBBrowserTest : public ContentBrowserTest {
 public:
  IndexedDBBrowserTest() {}

  void SimpleTest(const GURL& test_url, bool incognito = false) {
    // The test page will perform tests on IndexedDB, then navigate to either
    // a #pass or #fail ref.
    Shell* the_browser = incognito ? CreateOffTheRecordBrowser() : shell();

    LOG(INFO) << "Navigating to URL and blocking.";
    NavigateToURLBlockUntilNavigationsComplete(the_browser, test_url, 2);
    LOG(INFO) << "Navigation done.";
    std::string result = the_browser->web_contents()->GetURL().ref();
    if (result != "pass") {
      std::string js_result;
      ASSERT_TRUE(ExecuteScriptAndExtractString(
          the_browser->web_contents(),
          "window.domAutomationController.send(getLog())",
          &js_result));
      FAIL() << "Failed: " << js_result;
    }
  }

  void NavigateAndWaitForTitle(Shell* shell,
                               const char* filename,
                               const char* hash,
                               const char* expected_string) {
    GURL url = GetTestUrl("indexeddb", filename);
    if (hash)
      url = GURL(url.spec() + hash);

    string16 expected_title16(ASCIIToUTF16(expected_string));
    TitleWatcher title_watcher(shell->web_contents(), expected_title16);
    NavigateToURL(shell, url);
    EXPECT_EQ(expected_title16, title_watcher.WaitAndGetTitle());
  }

  scoped_refptr<IndexedDBContext> GetContext() {
    StoragePartition* partition =
        BrowserContext::GetDefaultStoragePartition(
            shell()->web_contents()->GetBrowserContext());
    return partition->GetIndexedDBContext();
  };
};

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, CursorTest) {
  SimpleTest(GetTestUrl("indexeddb", "cursor_test.html"));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, CursorTestIncognito) {
  SimpleTest(GetTestUrl("indexeddb", "cursor_test.html"),
             true /* incognito */);
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, CursorPrefetch) {
  SimpleTest(GetTestUrl("indexeddb", "cursor_prefetch.html"));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, IndexTest) {
  SimpleTest(GetTestUrl("indexeddb", "index_test.html"));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, KeyPathTest) {
  SimpleTest(GetTestUrl("indexeddb", "key_path_test.html"));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, TransactionGetTest) {
  SimpleTest(GetTestUrl("indexeddb", "transaction_get_test.html"));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, KeyTypesTest) {
  SimpleTest(GetTestUrl("indexeddb", "key_types_test.html"));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, ObjectStoreTest) {
  SimpleTest(GetTestUrl("indexeddb", "object_store_test.html"));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, DatabaseTest) {
  SimpleTest(GetTestUrl("indexeddb", "database_test.html"));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, TransactionTest) {
  SimpleTest(GetTestUrl("indexeddb", "transaction_test.html"));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, ValueSizeTest) {
  SimpleTest(GetTestUrl("indexeddb", "value_size_test.html"));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, DISABLED_DoesntHangTest) {
  SimpleTest(GetTestUrl("indexeddb", "transaction_run_forever.html"));
  CrashTab(shell()->web_contents());
  SimpleTest(GetTestUrl("indexeddb", "transaction_test.html"));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, Bug84933Test) {
  const GURL url = GetTestUrl("indexeddb", "bug_84933.html");

  // Just navigate to the URL. Test will crash if it fails.
  NavigateToURLBlockUntilNavigationsComplete(shell(), url, 1);
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, Bug106883Test) {
  const GURL url = GetTestUrl("indexeddb", "bug_106883.html");

  // Just navigate to the URL. Test will crash if it fails.
  NavigateToURLBlockUntilNavigationsComplete(shell(), url, 1);
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, Bug109187Test) {
  const GURL url = GetTestUrl("indexeddb", "bug_109187.html");

  // Just navigate to the URL. Test will crash if it fails.
  NavigateToURLBlockUntilNavigationsComplete(shell(), url, 1);
}

class IndexedDBBrowserTestWithLowQuota : public IndexedDBBrowserTest {
 public:
  virtual void SetUpOnMainThread() OVERRIDE {
    const int kInitialQuotaKilobytes = 5000;
    const int kTemporaryStorageQuotaMaxSize = kInitialQuotaKilobytes
        * 1024 * QuotaManager::kPerHostTemporaryPortion;
    SetTempQuota(
        kTemporaryStorageQuotaMaxSize,
        BrowserContext::GetDefaultStoragePartition(
            shell()->web_contents()->GetBrowserContext())->GetQuotaManager());
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
  SimpleTest(GetTestUrl("indexeddb", "quota_test.html"));
}

class IndexedDBBrowserTestWithGCExposed : public IndexedDBBrowserTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitchASCII(switches::kJavaScriptFlags, "--expose-gc");
  }
};

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTestWithGCExposed,
                       DatabaseCallbacksTest) {
  SimpleTest(GetTestUrl("indexeddb", "database_callbacks_first.html"));
}

static void CopyLevelDBToProfile(Shell* shell,
                                 scoped_refptr<IndexedDBContext> context,
                                 const std::string& test_directory) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  base::FilePath leveldb_dir(FILE_PATH_LITERAL("file__0.indexeddb.leveldb"));
  base::FilePath test_data_dir =
      GetTestFilePath("indexeddb", test_directory.c_str()).Append(leveldb_dir);
  IndexedDBContextImpl* context_impl =
      static_cast<IndexedDBContextImpl*>(context.get());
  base::FilePath dest = context_impl->data_path().Append(leveldb_dir);
  // If we don't create the destination directory first, the contents of the
  // leveldb directory are copied directly into profile/IndexedDB instead of
  // profile/IndexedDB/file__0.xxx/
  ASSERT_TRUE(file_util::CreateDirectory(dest));
  const bool kRecursive = true;
  ASSERT_TRUE(file_util::CopyDirectory(test_data_dir,
                                       context_impl->data_path(),
                                       kRecursive));
}

class IndexedDBBrowserTestWithPreexistingLevelDB : public IndexedDBBrowserTest {
 public:
  IndexedDBBrowserTestWithPreexistingLevelDB() : disk_usage_(-1) { }

  virtual void SetUpOnMainThread() OVERRIDE {
    scoped_refptr<IndexedDBContext> context = GetContext();
    BrowserThread::PostTask(
        BrowserThread::WEBKIT_DEPRECATED, FROM_HERE,
        base::Bind(&CopyLevelDBToProfile, shell(), context,
                   EnclosingLevelDBDir()));
    scoped_refptr<base::ThreadTestHelper> helper(
        new base::ThreadTestHelper(BrowserThread::GetMessageLoopProxyForThread(
            BrowserThread::WEBKIT_DEPRECATED)));
    ASSERT_TRUE(helper->Run());
  }

  virtual std::string EnclosingLevelDBDir() = 0;

 protected:
  virtual int64 RequestDiskUsage() {
    BrowserThread::PostTaskAndReplyWithResult(
        BrowserThread::WEBKIT_DEPRECATED, FROM_HERE,
        base::Bind(&IndexedDBContext::GetOriginDiskUsage, GetContext(),
            GURL("file:///")), base::Bind(
                &IndexedDBBrowserTestWithPreexistingLevelDB::DidGetDiskUsage,
                this));
    scoped_refptr<base::ThreadTestHelper> helper(
        new base::ThreadTestHelper(BrowserThread::GetMessageLoopProxyForThread(
            BrowserThread::WEBKIT_DEPRECATED)));
    EXPECT_TRUE(helper->Run());
    // Wait for DidGetDiskUsage to be called.
    MessageLoop::current()->RunUntilIdle();
    return disk_usage_;
  }
 private:
  virtual void DidGetDiskUsage(int64 bytes) {
    EXPECT_GT(bytes, 0);
    disk_usage_ = bytes;
  }

  int64 disk_usage_;
};

class IndexedDBBrowserTestWithVersion0Schema : public
    IndexedDBBrowserTestWithPreexistingLevelDB {
  virtual std::string EnclosingLevelDBDir() OVERRIDE {
    return "migration_from_0";
  }
};

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTestWithVersion0Schema, MigrationTest) {
  SimpleTest(GetTestUrl("indexeddb", "migration_test.html"));
}

class IndexedDBBrowserTestWithVersion123456Schema : public
    IndexedDBBrowserTestWithPreexistingLevelDB {
  virtual std::string EnclosingLevelDBDir() OVERRIDE {
    return "schema_version_123456";
  }
};

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTestWithVersion123456Schema,
                       DestroyTest) {
  int64 original_size = RequestDiskUsage();
  EXPECT_GT(original_size, 0);
  SimpleTest(GetTestUrl("indexeddb", "open_bad_db.html"));
  int64 new_size = RequestDiskUsage();
  EXPECT_NE(original_size, new_size);
}

class IndexedDBBrowserTestWithVersion987654SSVData : public
    IndexedDBBrowserTestWithPreexistingLevelDB {
  virtual std::string EnclosingLevelDBDir() OVERRIDE {
    return "ssv_version_987654";
  }
};

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTestWithVersion987654SSVData,
                       DestroyTest) {
  int64 original_size = RequestDiskUsage();
  EXPECT_GT(original_size, 0);
  SimpleTest(GetTestUrl("indexeddb", "open_bad_db.html"));
  int64 new_size = RequestDiskUsage();
  EXPECT_NE(original_size, new_size);
}

class IndexedDBBrowserTestWithCorruptLevelDB : public
    IndexedDBBrowserTestWithPreexistingLevelDB {
  virtual std::string EnclosingLevelDBDir() OVERRIDE {
    return "corrupt_leveldb";
  }
};

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTestWithCorruptLevelDB,
                       DestroyTest) {
  int64 original_size = RequestDiskUsage();
  EXPECT_GT(original_size, 0);
  SimpleTest(GetTestUrl("indexeddb", "open_bad_db.html"));
  int64 new_size = RequestDiskUsage();
  EXPECT_NE(original_size, new_size);
}

class IndexedDBBrowserTestWithMissingSSTFile : public
    IndexedDBBrowserTestWithPreexistingLevelDB {
  virtual std::string EnclosingLevelDBDir() OVERRIDE {
    return "missing_sst";
  }
};

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTestWithMissingSSTFile,
                       DestroyTest) {
  int64 original_size = RequestDiskUsage();
  EXPECT_GT(original_size, 0);
  SimpleTest(GetTestUrl("indexeddb", "open_bad_db.html"));
  int64 new_size = RequestDiskUsage();
  EXPECT_NE(original_size, new_size);
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, LevelDBLogFileTest) {
  // Any page that opens an IndexedDB will work here.
  SimpleTest(GetTestUrl("indexeddb", "database_test.html"));
  scoped_refptr<IndexedDBContext> context =
      BrowserContext::GetDefaultStoragePartition(
          shell()->web_contents()->GetBrowserContext())->
              GetIndexedDBContext();
  IndexedDBContextImpl* context_impl =
      static_cast<IndexedDBContextImpl*>(context.get());
  base::FilePath leveldb_dir(FILE_PATH_LITERAL("file__0.indexeddb.leveldb"));
  base::FilePath log_file(FILE_PATH_LITERAL("LOG"));
  base::FilePath log_file_path =
      context_impl->data_path().Append(leveldb_dir).Append(log_file);
  int64 size;
  EXPECT_TRUE(file_util::GetFileSize(log_file_path, &size));
  EXPECT_GT(size, 0);
}

// Complex multi-step (converted from pyauto) tests begin here.

// Verify null key path persists after restarting browser.
IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, PRE_NullKeyPathPersistence) {
  NavigateAndWaitForTitle(shell(), "bug_90635.html", "#part1",
                          "pass - first run");
}

// Verify null key path persists after restarting browser.
IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, NullKeyPathPersistence) {
  NavigateAndWaitForTitle(shell(), "bug_90635.html", "#part2",
                          "pass - second run");
}

// Verify that a VERSION_CHANGE transaction is rolled back after a
// renderer/browser crash
IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest,
                       PRE_PRE_VersionChangeCrashResilience) {
  NavigateAndWaitForTitle(shell(), "version_change_crash.html", "#part1",
                          "pass - part1 - complete");
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, PRE_VersionChangeCrashResilience) {
  NavigateAndWaitForTitle(shell(), "version_change_crash.html", "#part2",
                          "pass - part2 - crash me");
  NavigateToURL(shell(), GURL(chrome::kChromeUIBrowserCrashHost));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, VersionChangeCrashResilience) {
  NavigateAndWaitForTitle(shell(), "version_change_crash.html", "#part3",
                          "pass - part3 - rolled back");
}

// Verify that open DB connections are closed when a tab is destroyed.
IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, ConnectionsClosedOnTabClose) {
  NavigateAndWaitForTitle(shell(), "version_change_blocked.html", "#tab1",
                          "setVersion(2) complete");

  // Start on a different URL to force a new renderer process.
  Shell* new_shell = CreateBrowser();
  NavigateToURL(new_shell, GURL(chrome::kAboutBlankURL));
  NavigateAndWaitForTitle(new_shell, "version_change_blocked.html", "#tab2",
                          "setVersion(3) blocked");

  string16 expected_title16(ASCIIToUTF16("setVersion(3) complete"));
  TitleWatcher title_watcher(new_shell->web_contents(), expected_title16);

  base::KillProcess(
      shell()->web_contents()->GetRenderProcessHost()->GetHandle(), 0, true);
  shell()->Close();

  EXPECT_EQ(expected_title16, title_watcher.WaitAndGetTitle());
}

}  // namespace content
