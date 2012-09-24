// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains download browser tests that are known to be runnable
// in a pure content context.  Over time tests should be migrated here.

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/scoped_temp_dir.h"
#include "content/browser/download/download_file_factory.h"
#include "content/browser/download/download_file_impl.h"
#include "content/browser/download/download_file_manager.h"
#include "content/browser/download/download_item_impl.h"
#include "content/browser/download/download_manager_impl.h"
#include "content/browser/power_save_blocker.h"
#include "content/browser/renderer_host/resource_dispatcher_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/shell.h"
#include "content/shell/shell_browser_context.h"
#include "content/shell/shell_download_manager_delegate.h"
#include "content/test/content_browser_test.h"
#include "content/test/content_browser_test_utils.h"
#include "content/test/net/url_request_mock_http_job.h"
#include "content/test/net/url_request_slow_download_job.h"
#include "googleurl/src/gurl.h"

namespace content {

namespace {

class DownloadFileWithDelayFactory;

static DownloadManagerImpl* DownloadManagerForShell(Shell* shell) {
  // We're in a content_browsertest; we know that the DownloadManager
  // is a DownloadManagerImpl.
  return static_cast<DownloadManagerImpl*>(
      BrowserContext::GetDownloadManager(
          shell->web_contents()->GetBrowserContext()));
}

class DownloadFileWithDelay : public DownloadFileImpl {
 public:
  DownloadFileWithDelay(
      const DownloadCreateInfo* info,
      scoped_ptr<content::ByteStreamReader> stream,
      DownloadRequestHandleInterface* request_handle,
      scoped_refptr<content::DownloadManager> download_manager,
      bool calculate_hash,
      scoped_ptr<content::PowerSaveBlocker> power_save_blocker,
      const net::BoundNetLog& bound_net_log,
      // |owner| is required to outlive the DownloadFileWithDelay.
      DownloadFileWithDelayFactory* owner);

  virtual ~DownloadFileWithDelay();

  // Wraps DownloadFileImpl::Rename and intercepts the return callback,
  // storing it in the factory that produced this object for later
  // retrieval.
  virtual void Rename(const FilePath& full_path,
                      bool overwrite_existing_file,
                      const RenameCompletionCallback& callback) OVERRIDE;

  // Wraps DownloadFileImpl::Detach and intercepts the return callback,
  // storing it in the factory that produced this object for later
  // retrieval.
  virtual void Detach(base::Closure callback) OVERRIDE;

 private:
  static void RenameCallbackWrapper(
      DownloadFileWithDelayFactory* factory,
      const RenameCompletionCallback& original_callback,
      content::DownloadInterruptReason reason,
      const FilePath& path);

  static void DetachCallbackWrapper(
      DownloadFileWithDelayFactory* factory,
      const base::Closure& original_callback);

  // May only be used on the UI thread.
  DownloadFileWithDelayFactory* owner_;

  DISALLOW_COPY_AND_ASSIGN(DownloadFileWithDelay);
};

class DownloadFileWithDelayFactory : public DownloadFileFactory {
 public:
  DownloadFileWithDelayFactory();
  virtual ~DownloadFileWithDelayFactory();

  // DownloadFileFactory interface.
  virtual content::DownloadFile* CreateFile(
      DownloadCreateInfo* info,
      scoped_ptr<content::ByteStreamReader> stream,
      DownloadManager* download_manager,
      bool calculate_hash,
      const net::BoundNetLog& bound_net_log) OVERRIDE;

  // Must all be called on the UI thread.
  void AddRenameCallback(base::Closure callback);
  void AddDetachCallback(base::Closure callback);
  void GetAllRenameCallbacks(std::vector<base::Closure>* results);
  void GetAllDetachCallbacks(std::vector<base::Closure>* results);

  // Do not return until either GetAllRenameCallbacks() or
  // GetAllDetachCallbacks() will return a non-empty list.
  void WaitForSomeCallback();

 private:
  std::vector<base::Closure> rename_callbacks_;
  std::vector<base::Closure> detach_callbacks_;
  bool waiting_;

  DISALLOW_COPY_AND_ASSIGN(DownloadFileWithDelayFactory);
};

DownloadFileWithDelay::DownloadFileWithDelay(
      const DownloadCreateInfo* info,
      scoped_ptr<content::ByteStreamReader> stream,
      DownloadRequestHandleInterface* request_handle,
      scoped_refptr<content::DownloadManager> download_manager,
      bool calculate_hash,
      scoped_ptr<content::PowerSaveBlocker> power_save_blocker,
      const net::BoundNetLog& bound_net_log,
      DownloadFileWithDelayFactory* owner)
    : DownloadFileImpl(info, stream.Pass(), request_handle, download_manager,
                       calculate_hash, power_save_blocker.Pass(),
                       bound_net_log),
      owner_(owner) {}

DownloadFileWithDelay::~DownloadFileWithDelay() {}

void DownloadFileWithDelay::Rename(const FilePath& full_path,
                      bool overwrite_existing_file,
                      const RenameCompletionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DownloadFileImpl::Rename(
      full_path, overwrite_existing_file,
      base::Bind(DownloadFileWithDelay::RenameCallbackWrapper,
                 base::Unretained(owner_), callback));
}

void DownloadFileWithDelay::Detach(base::Closure callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DownloadFileImpl::Detach(
      base::Bind(DownloadFileWithDelay::DetachCallbackWrapper,
                 base::Unretained(owner_), callback));
}

// static
void DownloadFileWithDelay::RenameCallbackWrapper(
    DownloadFileWithDelayFactory* factory,
    const RenameCompletionCallback& original_callback,
    content::DownloadInterruptReason reason,
    const FilePath& path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  factory->AddRenameCallback(base::Bind(original_callback, reason, path));
}

// static
void DownloadFileWithDelay::DetachCallbackWrapper(
    DownloadFileWithDelayFactory* factory,
    const base::Closure& original_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  factory->AddDetachCallback(original_callback);
}

DownloadFileWithDelayFactory::DownloadFileWithDelayFactory()
    : waiting_(false) {}
DownloadFileWithDelayFactory::~DownloadFileWithDelayFactory() {}

DownloadFile* DownloadFileWithDelayFactory::CreateFile(
    DownloadCreateInfo* info,
    scoped_ptr<content::ByteStreamReader> stream,
    DownloadManager* download_manager,
    bool calculate_hash,
    const net::BoundNetLog& bound_net_log) {

  return new DownloadFileWithDelay(
      info, stream.Pass(), new DownloadRequestHandle(info->request_handle),
      download_manager, calculate_hash,
      scoped_ptr<content::PowerSaveBlocker>(
          new content::PowerSaveBlocker(
              content::PowerSaveBlocker::kPowerSaveBlockPreventAppSuspension,
              "Download in progress")).Pass(),
      bound_net_log, this);
}

void DownloadFileWithDelayFactory::AddRenameCallback(base::Closure callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  rename_callbacks_.push_back(callback);
  if (waiting_)
    MessageLoopForUI::current()->Quit();
}

void DownloadFileWithDelayFactory::AddDetachCallback(base::Closure callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  detach_callbacks_.push_back(callback);
  if (waiting_)
    MessageLoopForUI::current()->Quit();
}

void DownloadFileWithDelayFactory::GetAllRenameCallbacks(
    std::vector<base::Closure>* results) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  results->swap(rename_callbacks_);
}

void DownloadFileWithDelayFactory::GetAllDetachCallbacks(
    std::vector<base::Closure>* results) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  results->swap(detach_callbacks_);
}

void DownloadFileWithDelayFactory::WaitForSomeCallback() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (rename_callbacks_.empty() && detach_callbacks_.empty()) {
    waiting_ = true;
    RunMessageLoop();
    waiting_ = false;
  }
}

}  // namespace

class DownloadContentTest : public ContentBrowserTest {
 protected:
  virtual void SetUpOnMainThread() OVERRIDE {
    ASSERT_TRUE(downloads_directory_.CreateUniqueTempDir());

    ShellDownloadManagerDelegate* delegate =
        static_cast<ShellDownloadManagerDelegate*>(
            shell()->web_contents()->GetBrowserContext()
            ->GetDownloadManagerDelegate());
    delegate->SetDownloadBehaviorForTesting(downloads_directory_.path());

    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&URLRequestSlowDownloadJob::AddUrlHandler));
    FilePath mock_base(GetTestFilePath("download", ""));
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&URLRequestMockHTTPJob::AddUrlHandler, mock_base));
  }

  // Create a DownloadTestObserverTerminal that will wait for the
  // specified number of downloads to finish.
  DownloadTestObserver* CreateWaiter(
      Shell* shell, int num_downloads) {
    DownloadManager* download_manager = DownloadManagerForShell(shell);
    return new DownloadTestObserverTerminal(download_manager, num_downloads,
        DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);
  }

  // Create a DownloadTestObserverInProgress that will wait for the
  // specified number of downloads to start.
  DownloadTestObserver* CreateInProgressWaiter(
      Shell* shell, int num_downloads) {
    DownloadManager* download_manager = DownloadManagerForShell(shell);
    return new DownloadTestObserverInProgress(download_manager, num_downloads);
  }

  bool EnsureNoPendingDownloads() {
    bool result = true;
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&EnsureNoPendingDownloadJobsOnIO, &result));
    MessageLoop::current()->Run();
    return result && DownloadManager::EnsureNoPendingDownloadsForTesting();
  }

  void DownloadAndWait(Shell* shell, const GURL& url) {
    scoped_ptr<DownloadTestObserver> observer(CreateWaiter(shell, 1));
    NavigateToURL(shell, url);
    observer->WaitForFinished();
    EXPECT_EQ(1u, observer->NumDownloadsSeenInState(DownloadItem::COMPLETE));
  }

  // Checks that |path| is has |file_size| bytes, and matches the |value|
  // string.
  bool VerifyFile(const FilePath& path,
                  const std::string& value,
                  const int64 file_size) {
    std::string file_contents;

    bool read = file_util::ReadFileToString(path, &file_contents);
    EXPECT_TRUE(read) << "Failed reading file: " << path.value() << std::endl;
    if (!read)
      return false;  // Couldn't read the file.

    // Note: we don't handle really large files (more than size_t can hold)
    // so we will fail in that case.
    size_t expected_size = static_cast<size_t>(file_size);

    // Check the size.
    EXPECT_EQ(expected_size, file_contents.size());
    if (expected_size != file_contents.size())
      return false;

    // Check the contents.
    EXPECT_EQ(value, file_contents);
    if (memcmp(file_contents.c_str(), value.c_str(), expected_size) != 0)
      return false;

    return true;
  }

  DownloadFileManager* GetDownloadFileManager() {
    ResourceDispatcherHostImpl* rdh(ResourceDispatcherHostImpl::Get());
    return rdh->download_file_manager();
  }

 private:
  static void EnsureNoPendingDownloadJobsOnIO(bool* result) {
    if (URLRequestSlowDownloadJob::NumberOutstandingRequests())
      *result = false;
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE, MessageLoop::QuitClosure());
  }

  // Location of the downloads directory for these tests
  ScopedTempDir downloads_directory_;
};

IN_PROC_BROWSER_TEST_F(DownloadContentTest, DownloadCancelled) {
  // TODO(rdsmith): Fragile code warning!  The code below relies on
  // the DownloadTestObserverInProgress only finishing when the new
  // download has reached the state of being entered into the history
  // and being user-visible (that's what's required for the Remove to
  // be valid).  By the pure semantics of
  // DownloadTestObserverInProgress, that's not guaranteed;
  // DownloadItems are created in the IN_PROGRESS state and made known
  // to the DownloadManager immediately, so any ModelChanged event on
  // the DownloadManager after navigation would allow the observer to
  // return.  However, the only ModelChanged() event the code will
  // currently fire is in OnCreateDownloadEntryComplete, at which
  // point the download item will be in the state we need.
  // The right way to fix this is to create finer grained states on the
  // DownloadItem, and wait for the state that indicates the item has been
  // entered in the history and made visible in the UI.

  // Create a download, wait until it's started, and confirm
  // we're in the expected state.
  scoped_ptr<DownloadTestObserver> observer(CreateInProgressWaiter(shell(), 1));
  NavigateToURL(shell(), GURL(URLRequestSlowDownloadJob::kUnknownSizeUrl));
  observer->WaitForFinished();

  std::vector<DownloadItem*> downloads;
  DownloadManagerForShell(shell())->GetAllDownloads(&downloads);
  ASSERT_EQ(1u, downloads.size());
  ASSERT_EQ(DownloadItem::IN_PROGRESS, downloads[0]->GetState());

  // Cancel the download and wait for download system quiesce.
  downloads[0]->Delete(DownloadItem::DELETE_DUE_TO_USER_DISCARD);
  scoped_refptr<DownloadTestFlushObserver> flush_observer(
      new DownloadTestFlushObserver(DownloadManagerForShell(shell())));
  flush_observer->WaitForFlush();

  // Get the important info from other threads and check it.
  EXPECT_TRUE(EnsureNoPendingDownloads());
}

// Check that downloading multiple (in this case, 2) files does not result in
// corrupted files.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, MultiDownload) {
  // Create a download, wait until it's started, and confirm
  // we're in the expected state.
  scoped_ptr<DownloadTestObserver> observer1(
      CreateInProgressWaiter(shell(), 1));
  NavigateToURL(shell(), GURL(URLRequestSlowDownloadJob::kUnknownSizeUrl));
  observer1->WaitForFinished();

  std::vector<DownloadItem*> downloads;
  DownloadManagerForShell(shell())->GetAllDownloads(&downloads);
  ASSERT_EQ(1u, downloads.size());
  ASSERT_EQ(DownloadItem::IN_PROGRESS, downloads[0]->GetState());
  DownloadItem* download1 = downloads[0];  // The only download.

  // Start the second download and wait until it's done.
  FilePath file(FILE_PATH_LITERAL("download-test.lib"));
  GURL url(URLRequestMockHTTPJob::GetMockUrl(file));
  // Download the file and wait.
  DownloadAndWait(shell(), url);

  // Should now have 2 items on the manager.
  downloads.clear();
  DownloadManagerForShell(shell())->GetAllDownloads(&downloads);
  ASSERT_EQ(2u, downloads.size());
  // We don't know the order of the downloads.
  DownloadItem* download2 = downloads[(download1 == downloads[0]) ? 1 : 0];

  ASSERT_EQ(DownloadItem::IN_PROGRESS, download1->GetState());
  ASSERT_EQ(DownloadItem::COMPLETE, download2->GetState());

  // Allow the first request to finish.
  scoped_ptr<DownloadTestObserver> observer2(CreateWaiter(shell(), 1));
  NavigateToURL(shell(), GURL(URLRequestSlowDownloadJob::kFinishDownloadUrl));
  observer2->WaitForFinished();  // Wait for the third request.
  EXPECT_EQ(1u, observer2->NumDownloadsSeenInState(DownloadItem::COMPLETE));

  // Get the important info from other threads and check it.
  EXPECT_TRUE(EnsureNoPendingDownloads());

  // The |DownloadItem|s should now be done and have the final file names.
  // Verify that the files have the expected data and size.
  // |file1| should be full of '*'s, and |file2| should be the same as the
  // source file.
  FilePath file1(download1->GetFullPath());
  size_t file_size1 = URLRequestSlowDownloadJob::kFirstDownloadSize +
                      URLRequestSlowDownloadJob::kSecondDownloadSize;
  std::string expected_contents(file_size1, '*');
  ASSERT_TRUE(VerifyFile(file1, expected_contents, file_size1));

  FilePath file2(download2->GetFullPath());
  ASSERT_TRUE(file_util::ContentsEqual(
      file2, GetTestFilePath("download", "download-test.lib")));
}

// Try to cancel just before we release the download file, by delaying final
// rename callback.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, CancelAtFinalRename) {
  // Setup new factory.
  DownloadFileWithDelayFactory* file_factory =
      new DownloadFileWithDelayFactory();
  GetDownloadFileManager()->SetFileFactoryForTesting(
      scoped_ptr<content::DownloadFileFactory>(file_factory).Pass());

  // Create a download
  FilePath file(FILE_PATH_LITERAL("download-test.lib"));
  NavigateToURL(shell(), URLRequestMockHTTPJob::GetMockUrl(file));

  // Wait until the first (intermediate file) rename and execute the callback.
  file_factory->WaitForSomeCallback();
  std::vector<base::Closure> callbacks;
  file_factory->GetAllDetachCallbacks(&callbacks);
  ASSERT_TRUE(callbacks.empty());
  file_factory->GetAllRenameCallbacks(&callbacks);
  ASSERT_EQ(1u, callbacks.size());
  callbacks[0].Run();
  callbacks.clear();

  // Wait until the second (final) rename callback is posted.
  file_factory->WaitForSomeCallback();
  file_factory->GetAllDetachCallbacks(&callbacks);
  ASSERT_TRUE(callbacks.empty());
  file_factory->GetAllRenameCallbacks(&callbacks);
  ASSERT_EQ(1u, callbacks.size());

  // Cancel it.
  std::vector<DownloadItem*> items;
  DownloadManagerForShell(shell())->GetAllDownloads(&items);
  ASSERT_EQ(1u, items.size());
  items[0]->Cancel(true);
  RunAllPendingInMessageLoop();

  // Check state.
  EXPECT_EQ(DownloadItem::CANCELLED, items[0]->GetState());

  // Run final rename callback.
  callbacks[0].Run();
  callbacks.clear();

  // Check state.
  EXPECT_EQ(DownloadItem::CANCELLED, items[0]->GetState());
}

// Try to cancel just after we release the download file, by delaying
// release.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, CancelAtRelease) {
  // Setup new factory.
  // Setup new factory.
  DownloadFileWithDelayFactory* file_factory =
      new DownloadFileWithDelayFactory();
  GetDownloadFileManager()->SetFileFactoryForTesting(
      scoped_ptr<content::DownloadFileFactory>(file_factory).Pass());

  // Create a download
  FilePath file(FILE_PATH_LITERAL("download-test.lib"));
  NavigateToURL(shell(), URLRequestMockHTTPJob::GetMockUrl(file));

  // Wait until the first (intermediate file) rename and execute the callback.
  file_factory->WaitForSomeCallback();
  std::vector<base::Closure> callbacks;
  file_factory->GetAllDetachCallbacks(&callbacks);
  ASSERT_TRUE(callbacks.empty());
  file_factory->GetAllRenameCallbacks(&callbacks);
  ASSERT_EQ(1u, callbacks.size());
  callbacks[0].Run();
  callbacks.clear();

  // Wait until the second (final) rename callback is posted.
  file_factory->WaitForSomeCallback();
  file_factory->GetAllDetachCallbacks(&callbacks);
  ASSERT_TRUE(callbacks.empty());
  file_factory->GetAllRenameCallbacks(&callbacks);
  ASSERT_EQ(1u, callbacks.size());

  // Call it.
  callbacks[0].Run();
  callbacks.clear();

  // Confirm download isn't complete yet.
  std::vector<DownloadItem*> items;
  DownloadManagerForShell(shell())->GetAllDownloads(&items);
  EXPECT_EQ(DownloadItem::IN_PROGRESS, items[0]->GetState());

  // Cancel the download; confirm cancel fails anyway.
  ASSERT_EQ(1u, items.size());
  items[0]->Cancel(true);
  EXPECT_EQ(DownloadItem::IN_PROGRESS, items[0]->GetState());
  RunAllPendingInMessageLoop();
  EXPECT_EQ(DownloadItem::IN_PROGRESS, items[0]->GetState());

  // Confirm detach callback and run it.
  file_factory->WaitForSomeCallback();
  file_factory->GetAllRenameCallbacks(&callbacks);
  ASSERT_TRUE(callbacks.empty());
  file_factory->GetAllDetachCallbacks(&callbacks);
  ASSERT_EQ(1u, callbacks.size());
  callbacks[0].Run();
  callbacks.clear();
  EXPECT_EQ(DownloadItem::COMPLETE, items[0]->GetState());
}

}  // namespace content
