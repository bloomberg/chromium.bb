// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/test/thread_test_helper.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/shell/shell.h"
#include "content/test/content_browser_test.h"
#include "content/test/content_browser_test_utils.h"
#include "content/test/layout_browsertest.h"
#include "net/test/test_server.h"
#include "webkit/quota/quota_manager.h"

using quota::QuotaManager;

namespace content {

// This browser test is aimed towards exercising the FileAPI bindings and
// the actual implementation that lives in the browser side.
class FileSystemBrowserTest : public ContentBrowserTest {
 public:
  FileSystemBrowserTest() {}

  void SimpleTest(const GURL& test_url, bool incognito = false) {
    // The test page will perform tests on FileAPI, then navigate to either
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
};

class FileSystemBrowserTestWithLowQuota : public FileSystemBrowserTest {
 public:
  virtual void SetUpOnMainThread() OVERRIDE {
    const int kInitialQuotaKilobytes = 5000;
    const int kTemporaryStorageQuotaMaxSize =
        kInitialQuotaKilobytes * 1024 * QuotaManager::kPerHostTemporaryPortion;
    SetTempQuota(
        kTemporaryStorageQuotaMaxSize,
        BrowserContext::GetDefaultStoragePartition(
            shell()->web_contents()->GetBrowserContext())->GetQuotaManager());
  }

  static void SetTempQuota(int64 bytes, scoped_refptr<QuotaManager> qm) {
    if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          base::Bind(&FileSystemBrowserTestWithLowQuota::SetTempQuota, bytes,
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

// This class runs WebKit filesystem layout tests.
class FileSystemLayoutTest : public InProcessBrowserLayoutTest {
 public:
  FileSystemLayoutTest() :
      InProcessBrowserLayoutTest(
          base::FilePath(),
          base::FilePath(
              FILE_PATH_LITERAL("fast/filesystem")
                  ).NormalizePathSeparators()) {
  }
};

IN_PROC_BROWSER_TEST_F(FileSystemBrowserTest, RequestTest) {
  SimpleTest(GetTestUrl("fileapi", "request_test.html"));
}

IN_PROC_BROWSER_TEST_F(FileSystemBrowserTest, CreateTest) {
  SimpleTest(GetTestUrl("fileapi", "create_test.html"));
}

IN_PROC_BROWSER_TEST_F(FileSystemBrowserTestWithLowQuota, QuotaTest) {
  SimpleTest(GetTestUrl("fileapi", "quota_test.html"));
}

IN_PROC_BROWSER_TEST_F(FileSystemLayoutTest, AsyncOperations) {
  RunLayoutTest("async-operations.html");
}

// crbug.com/172787 for disabling on Windows (flaky), crbug.com/173079 for
// temporary disabling everywhere.
IN_PROC_BROWSER_TEST_F(FileSystemLayoutTest, DISABLED_CrossFilesystemOp) {
  RunLayoutTest("cross-filesystem-op.html");
}

IN_PROC_BROWSER_TEST_F(FileSystemLayoutTest, DirectoryEntryToUri) {
  RunLayoutTest("directory-entry-to-uri.html");
}

IN_PROC_BROWSER_TEST_F(FileSystemLayoutTest, EntryPointsMissingArguments) {
  RunLayoutTest("entry-points-missing-arguments.html");
}

IN_PROC_BROWSER_TEST_F(FileSystemLayoutTest, FileAfterReloadCrash) {
  RunLayoutTest("file-after-reload-crash.html");
}

IN_PROC_BROWSER_TEST_F(FileSystemLayoutTest, FileEntryToUri) {
  RunLayoutTest("file-entry-to-uri.html");
}

IN_PROC_BROWSER_TEST_F(FileSystemLayoutTest, FileFromFileEntry) {
  RunLayoutTest("file-from-file-entry.html");
}

IN_PROC_BROWSER_TEST_F(FileSystemLayoutTest, FileMetadataAfterWrite) {
  RunLayoutTest("file-metadata-after-write.html");
}

IN_PROC_BROWSER_TEST_F(FileSystemLayoutTest, FilesystemMissingArguments) {
  RunLayoutTest("filesystem-missing-arguments.html");
}

IN_PROC_BROWSER_TEST_F(FileSystemLayoutTest, FilesystemNoCallbackNullPtrCrash) {
  RunLayoutTest("filesystem-no-callback-null-ptr-crash.html");
}

IN_PROC_BROWSER_TEST_F(FileSystemLayoutTest, FilesystemReference) {
  RunLayoutTest("filesystem-reference.html");
}

IN_PROC_BROWSER_TEST_F(FileSystemLayoutTest, FilesystemUnserializable) {
  RunLayoutTest("filesystem-unserializable.html");
}

IN_PROC_BROWSER_TEST_F(FileSystemLayoutTest, FilesystemUriOrigin) {
  RunLayoutTest("filesystem-uri-origin.html");
}

IN_PROC_BROWSER_TEST_F(FileSystemLayoutTest, FileWriterAbortContinue) {
  RunLayoutTest("file-writer-abort-continue.html");
}

IN_PROC_BROWSER_TEST_F(FileSystemLayoutTest, FileWriterAbortDepth) {
  RunLayoutTest("file-writer-abort-depth.html");
}

IN_PROC_BROWSER_TEST_F(FileSystemLayoutTest, FileWriterAbort) {
  RunLayoutTest("file-writer-abort.html");
}

IN_PROC_BROWSER_TEST_F(FileSystemLayoutTest, FileWriterEmptyBlob) {
  RunLayoutTest("file-writer-empty-blob.html");
}

IN_PROC_BROWSER_TEST_F(FileSystemLayoutTest, FileWriterEvents) {
  RunLayoutTest("file-writer-events.html");
}

IN_PROC_BROWSER_TEST_F(FileSystemLayoutTest, FileWriterGcBlob) {
  RunLayoutTest("file-writer-gc-blob.html");
}

IN_PROC_BROWSER_TEST_F(FileSystemLayoutTest, FileWriterTruncateExtend) {
  RunLayoutTest("file-writer-truncate-extend.html");
}

IN_PROC_BROWSER_TEST_F(FileSystemLayoutTest, FileWriterWriteOverlapped) {
  RunLayoutTest("file-writer-write-overlapped.html");
}

IN_PROC_BROWSER_TEST_F(FileSystemLayoutTest, FlagsPassing) {
  RunLayoutTest("flags-passing.html");
}

IN_PROC_BROWSER_TEST_F(FileSystemLayoutTest, InputAccessEntries) {
  RunLayoutTest("input-access-entries.html");
}

IN_PROC_BROWSER_TEST_F(FileSystemLayoutTest, NotEnoughArguments) {
  RunLayoutTest("not-enough-arguments.html");
}

IN_PROC_BROWSER_TEST_F(FileSystemLayoutTest, OpCopy) {
  RunLayoutTest("op-copy.html");
}

IN_PROC_BROWSER_TEST_F(FileSystemLayoutTest, OpGetEntry) {
  RunLayoutTest("op-get-entry.html");
}

IN_PROC_BROWSER_TEST_F(FileSystemLayoutTest, OpGetMetadata) {
  RunLayoutTest("op-get-metadata.html");
}

IN_PROC_BROWSER_TEST_F(FileSystemLayoutTest, OpGetParent) {
  RunLayoutTest("op-get-parent.html");
}

IN_PROC_BROWSER_TEST_F(FileSystemLayoutTest, OpMove) {
  RunLayoutTest("op-move.html");
}

IN_PROC_BROWSER_TEST_F(FileSystemLayoutTest, OpReadDirectory) {
  RunLayoutTest("op-read-directory.html");
}

IN_PROC_BROWSER_TEST_F(FileSystemLayoutTest, OpRemove) {
  RunLayoutTest("op-remove.html");
}

// TODO(kinuko): This has been broken before but the bug has surfaced
// due to a partial fix for drive-letter handling.
// http://crbug.com/176253
IN_PROC_BROWSER_TEST_F(FileSystemLayoutTest, DISABLED_OpRestrictedChars) {
  RunLayoutTest("op-restricted-chars.html");
}

IN_PROC_BROWSER_TEST_F(FileSystemLayoutTest, OpRestrictedNames) {
  RunLayoutTest("op-restricted-names.html");
}

IN_PROC_BROWSER_TEST_F(FileSystemLayoutTest, OpRestrictedUnicode) {
  RunLayoutTest("op-restricted-unicode.html");
}

IN_PROC_BROWSER_TEST_F(FileSystemLayoutTest, ReadDirectory) {
  RunLayoutTest("read-directory.html");
}

IN_PROC_BROWSER_TEST_F(FileSystemLayoutTest, SimplePersistent) {
  RunLayoutTest("simple-persistent.html");
}

IN_PROC_BROWSER_TEST_F(FileSystemLayoutTest, SimpleReadonlyFileObject) {
  RunLayoutTest("simple-readonly-file-object.html");
}

IN_PROC_BROWSER_TEST_F(FileSystemLayoutTest, SimpleReadonly) {
  RunLayoutTest("simple-readonly.html");
}

IN_PROC_BROWSER_TEST_F(FileSystemLayoutTest,
    SimpleRequiredArgumentsGetdirectory) {
  RunLayoutTest("simple-required-arguments-getdirectory.html");
}

IN_PROC_BROWSER_TEST_F(FileSystemLayoutTest, SimpleRequiredArgumentsGetfile)
{
  RunLayoutTest("simple-required-arguments-getfile.html");
}

IN_PROC_BROWSER_TEST_F(FileSystemLayoutTest,
    SimpleRequiredArgumentsGetmetadata) {
  RunLayoutTest("simple-required-arguments-getmetadata.html");
}

IN_PROC_BROWSER_TEST_F(FileSystemLayoutTest, SimpleRequiredArgumentsRemove) {
  RunLayoutTest("simple-required-arguments-remove.html");
}

IN_PROC_BROWSER_TEST_F(FileSystemLayoutTest, SimpleTemporary) {
  RunLayoutTest("simple-temporary.html");
}

IN_PROC_BROWSER_TEST_F(FileSystemLayoutTest, WorkersAsyncOperations) {
  RunLayoutTest("workers/async-operations.html");
}

IN_PROC_BROWSER_TEST_F(FileSystemLayoutTest, WorkersDetachedFrameCrash) {
  RunLayoutTest("workers/detached-frame-crash.html");
}

IN_PROC_BROWSER_TEST_F(FileSystemLayoutTest, WorkersFileEntryToUriSync) {
  RunLayoutTest("workers/file-entry-to-uri-sync.html");
}

IN_PROC_BROWSER_TEST_F(FileSystemLayoutTest, WorkersFileFromFileEntry) {
  RunLayoutTest("workers/file-from-file-entry.html");
}

IN_PROC_BROWSER_TEST_F(FileSystemLayoutTest, WorkersFileFromFileEntrySync)
{
  RunLayoutTest("workers/file-from-file-entry-sync.html");
}

IN_PROC_BROWSER_TEST_F(FileSystemLayoutTest, WorkersFileWriterEmptyBlob) {
  RunLayoutTest("workers/file-writer-empty-blob.html");
}

IN_PROC_BROWSER_TEST_F(FileSystemLayoutTest, WorkersFileWriterEvents) {
  RunLayoutTest("workers/file-writer-events.html");
}

IN_PROC_BROWSER_TEST_F(FileSystemLayoutTest,
    WorkersFileWriterEventsSharedWorker) {
  RunLayoutTest("workers/file-writer-events-shared-worker.html");
}

IN_PROC_BROWSER_TEST_F(FileSystemLayoutTest, WorkersFileWriterGcBlob) {
  RunLayoutTest("workers/file-writer-gc-blob.html");
}

IN_PROC_BROWSER_TEST_F(FileSystemLayoutTest,
    WorkersFileWriterSyncTruncateExtend) {
  RunLayoutTest("workers/file-writer-sync-truncate-extend.html");
}

IN_PROC_BROWSER_TEST_F(FileSystemLayoutTest,
    WorkersFileWriterSyncWriteOverlapped) {
  RunLayoutTest("workers/file-writer-sync-write-overlapped.html");
}

IN_PROC_BROWSER_TEST_F(FileSystemLayoutTest, WorkersFileWriterTruncateExtend) {
  RunLayoutTest("workers/file-writer-truncate-extend.html");
}

IN_PROC_BROWSER_TEST_F(FileSystemLayoutTest, WorkersFileWriterWriteOverlapped) {
  RunLayoutTest("workers/file-writer-write-overlapped.html");
}

IN_PROC_BROWSER_TEST_F(FileSystemLayoutTest, WorkersSimplePersistent) {
  RunLayoutTest("workers/simple-persistent.html");
}

IN_PROC_BROWSER_TEST_F(FileSystemLayoutTest, WorkersSimplePersistentSync) {
  RunLayoutTest("workers/simple-persistent-sync.html");
}

IN_PROC_BROWSER_TEST_F(FileSystemLayoutTest, WorkersSimpleTemporary) {
  RunLayoutTest("workers/simple-temporary.html");
}

IN_PROC_BROWSER_TEST_F(FileSystemLayoutTest, WorkersSimpleTemporarySync) {
  RunLayoutTest("workers/simple-temporary-sync.html");
}

IN_PROC_BROWSER_TEST_F(FileSystemLayoutTest, WorkersSyncOperations) {
  RunLayoutTest("workers/sync-operations.html");
}

}  // namespace content
