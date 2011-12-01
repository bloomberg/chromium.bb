// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/threading/thread.h"
#include "chrome/test/base/testing_profile.h"
#include "content/browser/download/download_create_info.h"
#include "content/browser/download/download_id.h"
#include "content/browser/download/download_id_factory.h"
#include "content/browser/download/download_item.h"
#include "content/browser/download/download_request_handle.h"
#include "content/browser/download/download_status_updater.h"
#include "content/browser/download/interrupt_reasons.h"
#include "content/browser/download/mock_download_item.h"
#include "content/browser/download/mock_download_manager.h"
#include "content/browser/download/mock_download_manager_delegate.h"
#include "content/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

DownloadId::Domain kValidDownloadItemIdDomain = "valid DownloadId::Domain";

class DownloadItemTest : public testing::Test {
 public:
  class MockObserver : public DownloadItem::Observer {
   public:
    explicit MockObserver(DownloadItem* item) : item_(item), updated_(false) {
      item_->AddObserver(this);
    }
    ~MockObserver() { item_->RemoveObserver(this); }

    virtual void OnDownloadUpdated(DownloadItem* download) { updated_ = true; }

    virtual void OnDownloadOpened(DownloadItem* download) { }

    bool CheckUpdated() {
      bool was_updated = updated_;
      updated_ = false;
      return was_updated;
    }

   private:
    DownloadItem* item_;
    bool updated_;
  };

  DownloadItemTest()
      : id_factory_(new DownloadIdFactory(kValidDownloadItemIdDomain)),
        profile_(new TestingProfile()),
        ui_thread_(BrowserThread::UI, &loop_) {
  }

  ~DownloadItemTest() {
  }

  virtual void SetUp() {
    download_manager_delegate_.reset(new MockDownloadManagerDelegate());
    download_manager_ = new MockDownloadManager(
        download_manager_delegate_.get(),
        id_factory_,
        &download_status_updater_);
    download_manager_->Init(profile_.get());
  }

  virtual void TearDown() {
    download_manager_->Shutdown();
    // When a DownloadManager's reference count drops to 0, it is not
    // deleted immediately. Instead, a task is posted to the UI thread's
    // message loop to delete it.
    // So, drop the reference count to 0 and run the message loop once
    // to ensure that all resources are cleaned up before the test exits.
    download_manager_ = NULL;
    profile_.reset(NULL);
    ui_thread_.DeprecatedGetThreadObject()->message_loop()->RunAllPending();
  }

  DownloadItem* CreateDownloadItem(DownloadItem::DownloadState state) {
    // Normally, the download system takes ownership of info, and is
    // responsible for deleting it.  In these unit tests, however, we
    // don't call the function that deletes it, so we do so ourselves.
    scoped_ptr<DownloadCreateInfo> info_;

    info_.reset(new DownloadCreateInfo());
    info_->download_id = id_factory_->GetNextId();
    info_->prompt_user_for_save_location = false;
    info_->url_chain.push_back(GURL());
    info_->state = state;

    download_manager_->CreateDownloadItem(info_.get(), DownloadRequestHandle());
    return download_manager_->GetActiveDownloadItem(info_->download_id.local());
  }

 protected:
  DownloadStatusUpdater download_status_updater_;
  scoped_ptr<MockDownloadManagerDelegate> download_manager_delegate_;
  scoped_refptr<DownloadManager> download_manager_;

 private:
  scoped_refptr<DownloadIdFactory> id_factory_;
  scoped_ptr<TestingProfile> profile_;
  MessageLoopForUI loop_;
  // UI thread.
  content::TestBrowserThread ui_thread_;
};

namespace {

const int kDownloadChunkSize = 1000;
const int kDownloadSpeed = 1000;
const int kDummyDBHandle = 10;
const FilePath::CharType kDummyPath[] = FILE_PATH_LITERAL("/testpath");

} // namespace

// Tests to ensure calls that change a DownloadItem generate an update to
// observers.
// State changing functions not tested:
//  void OpenDownload();
//  void ShowDownloadInShell();
//  void CompleteDelayedDownload();
//  void OnDownloadCompleting(DownloadFileManager* file_manager);
//  void OnDownloadRenamedToFinalName(const FilePath& full_path);
//  set_* mutators

TEST_F(DownloadItemTest, NotificationAfterUpdate) {
  DownloadItem* item = CreateDownloadItem(DownloadItem::IN_PROGRESS);
  MockObserver observer(item);

  item->UpdateProgress(kDownloadChunkSize, kDownloadSpeed);
  ASSERT_TRUE(observer.CheckUpdated());
  EXPECT_EQ(kDownloadSpeed, item->CurrentSpeed());
}

TEST_F(DownloadItemTest, NotificationAfterCancel) {
  DownloadItem* user_cancel = CreateDownloadItem(DownloadItem::IN_PROGRESS);
  MockObserver observer1(user_cancel);

  user_cancel->Cancel(true);
  ASSERT_TRUE(observer1.CheckUpdated());

  DownloadItem* system_cancel = CreateDownloadItem(DownloadItem::IN_PROGRESS);
  MockObserver observer2(system_cancel);

  system_cancel->Cancel(false);
  ASSERT_TRUE(observer2.CheckUpdated());
}

TEST_F(DownloadItemTest, NotificationAfterComplete) {
  DownloadItem* item = CreateDownloadItem(DownloadItem::IN_PROGRESS);
  MockObserver observer(item);

  // Calling OnAllDataSaved does not trigger notification
  item->OnAllDataSaved(kDownloadChunkSize, DownloadItem::kEmptyFileHash);
  ASSERT_FALSE(observer.CheckUpdated());

  item->MarkAsComplete();
  ASSERT_TRUE(observer.CheckUpdated());
}

TEST_F(DownloadItemTest, NotificationAfterDownloadedFileRemoved) {
  DownloadItem* item = CreateDownloadItem(DownloadItem::IN_PROGRESS);
  MockObserver observer(item);

  item->OnDownloadedFileRemoved();
  ASSERT_TRUE(observer.CheckUpdated());
}

TEST_F(DownloadItemTest, NotificationAfterInterrupted) {
  DownloadItem* item = CreateDownloadItem(DownloadItem::IN_PROGRESS);
  MockObserver observer(item);

  item->Interrupted(kDownloadChunkSize, DOWNLOAD_INTERRUPT_REASON_NONE);
  ASSERT_TRUE(observer.CheckUpdated());
}

TEST_F(DownloadItemTest, NotificationAfterDelete) {
  DownloadItem* item = CreateDownloadItem(DownloadItem::IN_PROGRESS);
  MockObserver observer(item);

  item->Delete(DownloadItem::DELETE_DUE_TO_BROWSER_SHUTDOWN);
  ASSERT_TRUE(observer.CheckUpdated());
}

TEST_F(DownloadItemTest, NotificationAfterRemove) {
  DownloadItem* item = CreateDownloadItem(DownloadItem::IN_PROGRESS);
  MockObserver observer(item);

  item->Remove();
  ASSERT_TRUE(observer.CheckUpdated());
}

TEST_F(DownloadItemTest, NotificationAfterSetFileCheckResults) {
  // Setting to safe should not trigger any notifications
  DownloadItem* safe_item = CreateDownloadItem(DownloadItem::IN_PROGRESS);
  MockObserver safe_observer(safe_item);

  DownloadStateInfo state = safe_item->GetStateInfo();;
  state.danger = DownloadStateInfo::NOT_DANGEROUS;
  safe_item->SetFileCheckResults(state);
  ASSERT_FALSE(safe_observer.CheckUpdated());

  // Setting to unsafe url or unsafe file should trigger notification
  DownloadItem* unsafeurl_item = CreateDownloadItem(DownloadItem::IN_PROGRESS);
  MockObserver unsafeurl_observer(unsafeurl_item);

  state = unsafeurl_item->GetStateInfo();;
  state.danger = DownloadStateInfo::DANGEROUS_URL;
  unsafeurl_item->SetFileCheckResults(state);
  ASSERT_TRUE(unsafeurl_observer.CheckUpdated());

  unsafeurl_item->DangerousDownloadValidated();
  ASSERT_TRUE(unsafeurl_observer.CheckUpdated());

  DownloadItem* unsafefile_item = CreateDownloadItem(DownloadItem::IN_PROGRESS);
  MockObserver unsafefile_observer(unsafefile_item);

  state = unsafefile_item->GetStateInfo();;
  state.danger = DownloadStateInfo::DANGEROUS_FILE;
  unsafefile_item->SetFileCheckResults(state);
  ASSERT_TRUE(unsafefile_observer.CheckUpdated());

  unsafefile_item->DangerousDownloadValidated();
  ASSERT_TRUE(unsafefile_observer.CheckUpdated());
}

TEST_F(DownloadItemTest, NotificationAfterOnPathDetermined) {
  DownloadItem* item = CreateDownloadItem(DownloadItem::IN_PROGRESS);
  MockObserver observer(item);

  // Calling OnPathDetermined does not trigger notification
  item->OnPathDetermined(FilePath(kDummyPath));
  ASSERT_FALSE(observer.CheckUpdated());
}

TEST_F(DownloadItemTest, NotificationAfterRename) {
  DownloadItem* item = CreateDownloadItem(DownloadItem::IN_PROGRESS);
  MockObserver observer(item);

  // Calling Rename does not trigger notification
  item->Rename(FilePath(kDummyPath));
  ASSERT_FALSE(observer.CheckUpdated());
}

TEST_F(DownloadItemTest, NotificationAfterTogglePause) {
  DownloadItem* item = CreateDownloadItem(DownloadItem::IN_PROGRESS);
  MockObserver observer(item);

  item->TogglePause();
  ASSERT_TRUE(observer.CheckUpdated());

  item->TogglePause();
  ASSERT_TRUE(observer.CheckUpdated());
}

TEST(MockDownloadItem, Compiles) {
  MockDownloadItem mock_item;
}
