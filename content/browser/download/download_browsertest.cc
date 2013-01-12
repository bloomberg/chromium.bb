// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains download browser tests that are known to be runnable
// in a pure content context.  Over time tests should be migrated here.

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "content/browser/download/download_file_factory.h"
#include "content/browser/download/download_file_impl.h"
#include "content/browser/download/download_item_impl.h"
#include "content/browser/download/download_manager_impl.h"
#include "content/browser/power_save_blocker.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_switches.h"
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
#include "net/test/test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AllOf;
using ::testing::Field;
using ::testing::InSequence;
using ::testing::Property;
using ::testing::Return;
using ::testing::StrictMock;

namespace content {

namespace {

class MockDownloadItemObserver : public DownloadItem::Observer {
 public:
  MockDownloadItemObserver() {}
  virtual ~MockDownloadItemObserver() {}

  MOCK_METHOD1(OnDownloadUpdated, void(DownloadItem*));
  MOCK_METHOD1(OnDownloadOpened, void(DownloadItem*));
  MOCK_METHOD1(OnDownloadRemoved, void(DownloadItem*));
  MOCK_METHOD1(OnDownloadDestroyed, void(DownloadItem*));
};

class MockDownloadManagerObserver : public DownloadManager::Observer {
 public:
  MockDownloadManagerObserver() {}
  virtual ~MockDownloadManagerObserver() {}

  MOCK_METHOD2(OnDownloadCreated, void(DownloadManager*, DownloadItem*));
  MOCK_METHOD1(ModelChanged, void(DownloadManager*));
  MOCK_METHOD1(ManagerGoingDown, void(DownloadManager*));
};

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
      scoped_ptr<DownloadSaveInfo> save_info,
      const FilePath& default_download_directory,
      const GURL& url,
      const GURL& referrer_url,
      bool calculate_hash,
      scoped_ptr<ByteStreamReader> stream,
      const net::BoundNetLog& bound_net_log,
      scoped_ptr<PowerSaveBlocker> power_save_blocker,
      base::WeakPtr<DownloadDestinationObserver> observer,
      base::WeakPtr<DownloadFileWithDelayFactory> owner);

  virtual ~DownloadFileWithDelay();

  // Wraps DownloadFileImpl::Rename* and intercepts the return callback,
  // storing it in the factory that produced this object for later
  // retrieval.
  virtual void RenameAndUniquify(
      const FilePath& full_path,
      const RenameCompletionCallback& callback) OVERRIDE;
  virtual void RenameAndAnnotate(
      const FilePath& full_path,
      const RenameCompletionCallback& callback) OVERRIDE;

 private:
  static void RenameCallbackWrapper(
      DownloadFileWithDelayFactory* factory,
      const RenameCompletionCallback& original_callback,
      DownloadInterruptReason reason,
      const FilePath& path);

  // This variable may only be read on the FILE thread, and may only be
  // indirected through (e.g. methods on DownloadFileWithDelayFactory called)
  // on the UI thread.  This is because after construction,
  // DownloadFileWithDelay lives on the file thread, but
  // DownloadFileWithDelayFactory is purely a UI thread object.
  base::WeakPtr<DownloadFileWithDelayFactory> owner_;

  DISALLOW_COPY_AND_ASSIGN(DownloadFileWithDelay);
};

// All routines on this class must be called on the UI thread.
class DownloadFileWithDelayFactory : public DownloadFileFactory {
 public:
  DownloadFileWithDelayFactory();
  virtual ~DownloadFileWithDelayFactory();

  // DownloadFileFactory interface.
  virtual DownloadFile* CreateFile(
      scoped_ptr<DownloadSaveInfo> save_info,
      const FilePath& default_download_directory,
      const GURL& url,
      const GURL& referrer_url,
      bool calculate_hash,
      scoped_ptr<ByteStreamReader> stream,
      const net::BoundNetLog& bound_net_log,
      base::WeakPtr<DownloadDestinationObserver> observer) OVERRIDE;

  void AddRenameCallback(base::Closure callback);
  void GetAllRenameCallbacks(std::vector<base::Closure>* results);

  // Do not return until GetAllRenameCallbacks() will return a non-empty list.
  void WaitForSomeCallback();

 private:
  base::WeakPtrFactory<DownloadFileWithDelayFactory> weak_ptr_factory_;
  std::vector<base::Closure> rename_callbacks_;
  bool waiting_;

  DISALLOW_COPY_AND_ASSIGN(DownloadFileWithDelayFactory);
};

DownloadFileWithDelay::DownloadFileWithDelay(
    scoped_ptr<DownloadSaveInfo> save_info,
    const FilePath& default_download_directory,
    const GURL& url,
    const GURL& referrer_url,
    bool calculate_hash,
    scoped_ptr<ByteStreamReader> stream,
    const net::BoundNetLog& bound_net_log,
    scoped_ptr<PowerSaveBlocker> power_save_blocker,
    base::WeakPtr<DownloadDestinationObserver> observer,
    base::WeakPtr<DownloadFileWithDelayFactory> owner)
    : DownloadFileImpl(
        save_info.Pass(), default_download_directory, url, referrer_url,
        calculate_hash, stream.Pass(), bound_net_log,
        power_save_blocker.Pass(), observer),
      owner_(owner) {}

DownloadFileWithDelay::~DownloadFileWithDelay() {}

void DownloadFileWithDelay::RenameAndUniquify(
    const FilePath& full_path, const RenameCompletionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DownloadFileImpl::RenameAndUniquify(
      full_path, base::Bind(DownloadFileWithDelay::RenameCallbackWrapper,
                            owner_, callback));
}

void DownloadFileWithDelay::RenameAndAnnotate(
    const FilePath& full_path, const RenameCompletionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DownloadFileImpl::RenameAndAnnotate(
      full_path, base::Bind(DownloadFileWithDelay::RenameCallbackWrapper,
                            owner_, callback));
}

// static
void DownloadFileWithDelay::RenameCallbackWrapper(
    DownloadFileWithDelayFactory* factory,
    const RenameCompletionCallback& original_callback,
    DownloadInterruptReason reason,
    const FilePath& path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  factory->AddRenameCallback(base::Bind(original_callback, reason, path));
}

DownloadFileWithDelayFactory::DownloadFileWithDelayFactory()
    : weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      waiting_(false) {}
DownloadFileWithDelayFactory::~DownloadFileWithDelayFactory() {}

DownloadFile* DownloadFileWithDelayFactory::CreateFile(
    scoped_ptr<DownloadSaveInfo> save_info,
    const FilePath& default_download_directory,
    const GURL& url,
    const GURL& referrer_url,
    bool calculate_hash,
    scoped_ptr<ByteStreamReader> stream,
    const net::BoundNetLog& bound_net_log,
    base::WeakPtr<DownloadDestinationObserver> observer) {
  scoped_ptr<PowerSaveBlocker> psb(
      new PowerSaveBlocker(
          PowerSaveBlocker::kPowerSaveBlockPreventAppSuspension,
          "Download in progress"));
  return new DownloadFileWithDelay(
      save_info.Pass(), default_download_directory, url, referrer_url,
      calculate_hash, stream.Pass(), bound_net_log,
      psb.Pass(), observer, weak_ptr_factory_.GetWeakPtr());
}

void DownloadFileWithDelayFactory::AddRenameCallback(base::Closure callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  rename_callbacks_.push_back(callback);
  if (waiting_)
    MessageLoopForUI::current()->Quit();
}

void DownloadFileWithDelayFactory::GetAllRenameCallbacks(
    std::vector<base::Closure>* results) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  results->swap(rename_callbacks_);
}

void DownloadFileWithDelayFactory::WaitForSomeCallback() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (rename_callbacks_.empty()) {
    waiting_ = true;
    RunMessageLoop();
    waiting_ = false;
  }
}

class CountingDownloadFile : public DownloadFileImpl {
 public:
  CountingDownloadFile(
    scoped_ptr<DownloadSaveInfo> save_info,
    const FilePath& default_downloads_directory,
    const GURL& url,
    const GURL& referrer_url,
    bool calculate_hash,
    scoped_ptr<ByteStreamReader> stream,
    const net::BoundNetLog& bound_net_log,
    scoped_ptr<PowerSaveBlocker> power_save_blocker,
    base::WeakPtr<DownloadDestinationObserver> observer)
      : DownloadFileImpl(save_info.Pass(), default_downloads_directory,
                         url, referrer_url, calculate_hash,
                         stream.Pass(), bound_net_log,
                         power_save_blocker.Pass(), observer) {}

  virtual ~CountingDownloadFile() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    active_files_--;
  }

  virtual void Initialize(const InitializeCallback& callback) OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    active_files_++;
    return DownloadFileImpl::Initialize(callback);
  }

  static void GetNumberActiveFiles(int* result) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    *result = active_files_;
  }

  // Can be called on any thread, and will block (running message loop)
  // until data is returned.
  static int GetNumberActiveFilesFromFileThread() {
    int result = -1;
    BrowserThread::PostTaskAndReply(BrowserThread::FILE, FROM_HERE,
        base::Bind(&CountingDownloadFile::GetNumberActiveFiles, &result),
        MessageLoop::current()->QuitClosure());
    MessageLoop::current()->Run();
    DCHECK_NE(-1, result);
    return result;
  }

 private:
  static int active_files_;
};

int CountingDownloadFile::active_files_ = 0;

class CountingDownloadFileFactory : public DownloadFileFactory {
 public:
  CountingDownloadFileFactory() {}
  virtual ~CountingDownloadFileFactory() {}

  // DownloadFileFactory interface.
  virtual DownloadFile* CreateFile(
    scoped_ptr<DownloadSaveInfo> save_info,
    const FilePath& default_downloads_directory,
    const GURL& url,
    const GURL& referrer_url,
    bool calculate_hash,
    scoped_ptr<ByteStreamReader> stream,
    const net::BoundNetLog& bound_net_log,
    base::WeakPtr<DownloadDestinationObserver> observer) OVERRIDE {
    scoped_ptr<PowerSaveBlocker> psb(
        new PowerSaveBlocker(
            PowerSaveBlocker::kPowerSaveBlockPreventAppSuspension,
            "Download in progress"));
    return new CountingDownloadFile(
        save_info.Pass(), default_downloads_directory, url, referrer_url,
        calculate_hash, stream.Pass(), bound_net_log,
        psb.Pass(), observer);
  }
};

class TestShellDownloadManagerDelegate : public ShellDownloadManagerDelegate {
 public:
  TestShellDownloadManagerDelegate()
      : delay_download_open_(false) {}

  virtual bool ShouldOpenDownload(
      DownloadItem* item,
      const DownloadOpenDelayedCallback& callback) OVERRIDE {
    if (delay_download_open_) {
      delayed_callbacks_.push_back(callback);
      return false;
    }
    return true;
  }

  void SetDelayedOpen(bool delay) {
    delay_download_open_ = delay;
  }

  void GetDelayedCallbacks(
      std::vector<DownloadOpenDelayedCallback>* callbacks) {
    callbacks->swap(delayed_callbacks_);
  }
 private:
  virtual ~TestShellDownloadManagerDelegate() {}

  bool delay_download_open_;
  std::vector<DownloadOpenDelayedCallback> delayed_callbacks_;
};

// Filter for handling resumption; don't return true until
// we see first a transition to IN_PROGRESS then a transition to
// some final state.  Since this is a stateful filter, we
// must provide a flag in which to store the state; that flag
// must be false on initialization.  The flag must be the first argument
// so that Bind() can be used to produce the callback expected by
// DownloadUpdatedObserver.
bool DownloadResumptionFilter(bool* state_flag, DownloadItem* download) {
  if (*state_flag && DownloadItem::IN_PROGRESS != download->GetState()) {
    return true;
  }

  if (DownloadItem::IN_PROGRESS == download->GetState())
    *state_flag = true;

  return false;
}

// Filter for waiting for intermediate file rename.
bool IntermediateFileRenameFilter(DownloadItem* download) {
  return !download->GetFullPath().empty();
}

}  // namespace

class DownloadContentTest : public ContentBrowserTest {
 protected:
  virtual void SetUpOnMainThread() OVERRIDE {
    ASSERT_TRUE(downloads_directory_.CreateUniqueTempDir());

    TestShellDownloadManagerDelegate* delegate =
        new TestShellDownloadManagerDelegate();
    delegate->SetDownloadBehaviorForTesting(downloads_directory_.path());
    DownloadManager* manager = DownloadManagerForShell(shell());
    manager->SetDelegate(delegate);
    delegate->SetDownloadManager(manager);

    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&URLRequestSlowDownloadJob::AddUrlHandler));
    FilePath mock_base(GetTestFilePath("download", ""));
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&URLRequestMockHTTPJob::AddUrlHandler, mock_base));
  }

  TestShellDownloadManagerDelegate* GetDownloadManagerDelegate(
      DownloadManager* manager) {
    return static_cast<TestShellDownloadManagerDelegate*>(
        manager->GetDelegate());
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

  // Note: Cannot be used with other alternative DownloadFileFactorys
  void SetupEnsureNoPendingDownloads() {
    DownloadManagerForShell(shell())->SetDownloadFileFactoryForTesting(
        scoped_ptr<DownloadFileFactory>(
            new CountingDownloadFileFactory()).Pass());
  }

  bool EnsureNoPendingDownloads() {
    bool result = true;
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&EnsureNoPendingDownloadJobsOnIO, &result));
    MessageLoop::current()->Run();
    return result &&
        (CountingDownloadFile::GetNumberActiveFilesFromFileThread() == 0);
  }

  void DownloadAndWait(Shell* shell, const GURL& url,
                       DownloadItem::DownloadState expected_terminal_state) {
    scoped_ptr<DownloadTestObserver> observer(CreateWaiter(shell, 1));
    NavigateToURL(shell, url);
    observer->WaitForFinished();
    EXPECT_EQ(1u, observer->NumDownloadsSeenInState(expected_terminal_state));
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

 private:
  static void EnsureNoPendingDownloadJobsOnIO(bool* result) {
    if (URLRequestSlowDownloadJob::NumberOutstandingRequests())
      *result = false;
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE, MessageLoop::QuitClosure());
  }

  // Location of the downloads directory for these tests
  base::ScopedTempDir downloads_directory_;
};

IN_PROC_BROWSER_TEST_F(DownloadContentTest, DownloadCancelled) {
  SetupEnsureNoPendingDownloads();

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
  SetupEnsureNoPendingDownloads();

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
  DownloadAndWait(shell(), url, DownloadItem::COMPLETE);

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
  DownloadManagerImpl* download_manager(DownloadManagerForShell(shell()));
  download_manager->SetDownloadFileFactoryForTesting(
      scoped_ptr<DownloadFileFactory>(file_factory).Pass());

  // Create a download
  FilePath file(FILE_PATH_LITERAL("download-test.lib"));
  NavigateToURL(shell(), URLRequestMockHTTPJob::GetMockUrl(file));

  // Wait until the first (intermediate file) rename and execute the callback.
  file_factory->WaitForSomeCallback();
  std::vector<base::Closure> callbacks;
  file_factory->GetAllRenameCallbacks(&callbacks);
  ASSERT_EQ(1u, callbacks.size());
  callbacks[0].Run();
  callbacks.clear();

  // Wait until the second (final) rename callback is posted.
  file_factory->WaitForSomeCallback();
  file_factory->GetAllRenameCallbacks(&callbacks);
  ASSERT_EQ(1u, callbacks.size());

  // Cancel it.
  std::vector<DownloadItem*> items;
  download_manager->GetAllDownloads(&items);
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
// in ShouldOpenDownload.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, CancelAtRelease) {
  DownloadManagerImpl* download_manager(DownloadManagerForShell(shell()));

  // Mark delegate for delayed open.
  GetDownloadManagerDelegate(download_manager)->SetDelayedOpen(true);

  // Setup new factory.
  DownloadFileWithDelayFactory* file_factory =
      new DownloadFileWithDelayFactory();
  download_manager->SetDownloadFileFactoryForTesting(
      scoped_ptr<DownloadFileFactory>(file_factory).Pass());

  // Create a download
  FilePath file(FILE_PATH_LITERAL("download-test.lib"));
  NavigateToURL(shell(), URLRequestMockHTTPJob::GetMockUrl(file));

  // Wait until the first (intermediate file) rename and execute the callback.
  file_factory->WaitForSomeCallback();
  std::vector<base::Closure> callbacks;
  file_factory->GetAllRenameCallbacks(&callbacks);
  ASSERT_EQ(1u, callbacks.size());
  callbacks[0].Run();
  callbacks.clear();

  // Wait until the second (final) rename callback is posted.
  file_factory->WaitForSomeCallback();
  file_factory->GetAllRenameCallbacks(&callbacks);
  ASSERT_EQ(1u, callbacks.size());

  // Call it.
  callbacks[0].Run();
  callbacks.clear();

  // Confirm download still IN_PROGRESS (internal state COMPLETING).
  std::vector<DownloadItem*> items;
  download_manager->GetAllDownloads(&items);
  EXPECT_EQ(DownloadItem::IN_PROGRESS, items[0]->GetState());

  // Cancel the download; confirm cancel fails.
  ASSERT_EQ(1u, items.size());
  items[0]->Cancel(true);
  EXPECT_EQ(DownloadItem::IN_PROGRESS, items[0]->GetState());

  // Need to complete open test.
  std::vector<DownloadOpenDelayedCallback> delayed_callbacks;
  GetDownloadManagerDelegate(download_manager)->GetDelayedCallbacks(
      &delayed_callbacks);
  ASSERT_EQ(1u, delayed_callbacks.size());
  delayed_callbacks[0].Run(true);

  // *Now* the download should be complete.
  EXPECT_EQ(DownloadItem::COMPLETE, items[0]->GetState());
}

// Try to shutdown with a download in progress to make sure shutdown path
// works properly.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, ShutdownInProgress) {
  // Create a download that won't complete.
  scoped_ptr<DownloadTestObserver> observer(CreateInProgressWaiter(shell(), 1));
  NavigateToURL(shell(), GURL(URLRequestSlowDownloadJob::kUnknownSizeUrl));
  observer->WaitForFinished();

  // Get the item.
  std::vector<DownloadItem*> items;
  DownloadManagerForShell(shell())->GetAllDownloads(&items);
  ASSERT_EQ(1u, items.size());
  EXPECT_EQ(DownloadItem::IN_PROGRESS, items[0]->GetState());

  // Shutdown the download manager and make sure we get the right
  // notifications in the right order.
  StrictMock<MockDownloadItemObserver> item_observer;
  items[0]->AddObserver(&item_observer);
  MockDownloadManagerObserver manager_observer;
  // Don't care about ModelChanged() events.
  EXPECT_CALL(manager_observer, ModelChanged(_))
      .WillRepeatedly(Return());
  DownloadManagerForShell(shell())->AddObserver(&manager_observer);
  {
    InSequence notifications;

    EXPECT_CALL(manager_observer, ManagerGoingDown(
        DownloadManagerForShell(shell())))
        .WillOnce(Return());
    EXPECT_CALL(item_observer, OnDownloadUpdated(
        AllOf(items[0],
              Property(&DownloadItem::GetState, DownloadItem::CANCELLED))))
        .WillOnce(Return());
    EXPECT_CALL(item_observer, OnDownloadDestroyed(items[0]))
        .WillOnce(Return());
  }
  DownloadManagerForShell(shell())->Shutdown();
  items.clear();
}

// Try to shutdown just after we release the download file, by delaying
// release.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, ShutdownAtRelease) {
  DownloadManagerImpl* download_manager(DownloadManagerForShell(shell()));

  // Mark delegate for delayed open.
  GetDownloadManagerDelegate(download_manager)->SetDelayedOpen(true);

  // Setup new factory.
  DownloadFileWithDelayFactory* file_factory =
      new DownloadFileWithDelayFactory();
  download_manager->SetDownloadFileFactoryForTesting(
      scoped_ptr<DownloadFileFactory>(file_factory).Pass());

  // Create a download
  FilePath file(FILE_PATH_LITERAL("download-test.lib"));
  NavigateToURL(shell(), URLRequestMockHTTPJob::GetMockUrl(file));

  // Wait until the first (intermediate file) rename and execute the callback.
  file_factory->WaitForSomeCallback();
  std::vector<base::Closure> callbacks;
  file_factory->GetAllRenameCallbacks(&callbacks);
  ASSERT_EQ(1u, callbacks.size());
  callbacks[0].Run();
  callbacks.clear();

  // Wait until the second (final) rename callback is posted.
  file_factory->WaitForSomeCallback();
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

  MockDownloadItemObserver observer;
  items[0]->AddObserver(&observer);
  EXPECT_CALL(observer, OnDownloadDestroyed(items[0]));

  // Shutdown the download manager.  Mostly this is confirming a lack of
  // crashes.
  DownloadManagerForShell(shell())->Shutdown();
}

IN_PROC_BROWSER_TEST_F(DownloadContentTest, ResumeInterruptedDownload) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableDownloadResumption);
  ASSERT_TRUE(test_server()->Start());
  // Default behavior is a 15K file with a RST at 10K.  We request
  // a hold on the response so that we can get the target name determined
  // before handling the RST.
  // TODO(rdsmith): Figure out how to handle the races needed
  // so that we don't have to wait for the target name determination.
  GURL url = test_server()->GetURL("rangereset?hold");

  // Download and wait for file determination.
  scoped_ptr<DownloadTestObserver> observer(CreateInProgressWaiter(shell(), 1));
  NavigateToURL(shell(), url);
  observer->WaitForFinished();

  std::vector<DownloadItem*> downloads;
  DownloadManagerForShell(shell())->GetAllDownloads(&downloads);
  ASSERT_EQ(1u, downloads.size());
  DownloadItem* download(downloads[0]);
  if (download->GetFullPath().empty()) {
    DownloadUpdatedObserver intermediate_observer(
        download, base::Bind(&IntermediateFileRenameFilter));
    intermediate_observer.WaitForEvent();
  }

  // Unleash the RST!
  scoped_ptr<DownloadTestObserver> rst_observer(CreateWaiter(shell(), 1));
  GURL release_url = test_server()->GetURL("download-finish");
  NavigateToURL(shell(), release_url);
  rst_observer->WaitForFinished();
  EXPECT_EQ(DownloadItem::INTERRUPTED, download->GetState());
  EXPECT_EQ(4000u, download->GetReceivedBytes());
  EXPECT_EQ(8000u, download->GetTotalBytes());
  EXPECT_EQ(FILE_PATH_LITERAL("rangereset.crdownload"),
            download->GetFullPath().BaseName().value());

  {
    std::string file_contents;
    std::string expected_contents(4000, 'X');
    ASSERT_TRUE(file_util::ReadFileToString(
        download->GetFullPath(), &file_contents));
    EXPECT_EQ(4000u, file_contents.size());
    // In conditional to avoid spamming the console with two 4000 char strings.
    if (expected_contents != file_contents)
      EXPECT_TRUE(false) << "File contents do not have expected value.";
  }

  // Resume; should get entire file (note that a restart will fail
  // here because it'll produce another RST).
  bool flag(false);
  DownloadUpdatedObserver complete_observer(
      download, base::Bind(&DownloadResumptionFilter, &flag));
  download->ResumeInterruptedDownload();
  NavigateToURL(shell(), release_url);  // Needed to get past hold.
  complete_observer.WaitForEvent();
  EXPECT_EQ(DownloadItem::COMPLETE, download->GetState());
  EXPECT_EQ(8000u, download->GetReceivedBytes());
  EXPECT_EQ(8000u, download->GetTotalBytes());
  EXPECT_EQ(FILE_PATH_LITERAL("rangereset"),
            download->GetFullPath().BaseName().value())
      << "Target path name: " << download->GetTargetFilePath().value();

  {
    std::string file_contents;
    std::string expected_contents(8000, 'X');
    ASSERT_TRUE(file_util::ReadFileToString(
        download->GetFullPath(), &file_contents));
    EXPECT_EQ(8000u, file_contents.size());
    // In conditional to avoid spamming the console with two 800 char strings.
    if (expected_contents != file_contents)
      EXPECT_TRUE(false) << "File contents do not have expected value.";
  }
}

}  // namespace content
