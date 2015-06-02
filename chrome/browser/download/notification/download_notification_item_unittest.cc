// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/notification/download_notification_item.h"

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/test_simple_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/browser/notifications/notification_test_util.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/mock_download_item.h"
#include "content/public/test/mock_download_manager.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/fake_message_center.h"

using testing::NiceMock;
using testing::Return;
using testing::_;

namespace {

class MockDownloadNotificationItemDelegate
    : public DownloadNotificationItem::Delegate {
 public:
  MockDownloadNotificationItemDelegate()
      : on_download_removed_call_count_(0u),
        on_download_started_call_count_(0u),
        on_download_stopped_call_count_(0u) {}

  void OnCreated(DownloadNotificationItem* item) override {
  }

  void OnDownloadRemoved(DownloadNotificationItem* item) override {
    on_download_removed_call_count_++;
  }

  void OnDownloadStarted(DownloadNotificationItem* item) override {
    on_download_started_call_count_++;
  }

  void OnDownloadStopped(DownloadNotificationItem* item) override {
    on_download_stopped_call_count_++;
  }

  size_t GetOnDownloadRemovedCallCount() {
    return on_download_removed_call_count_;
  }

  size_t GetOnDownloadStartedCallCount() {
    return on_download_started_call_count_;
  }

  size_t GetOnDownloadStoppedCallCount() {
    return on_download_stopped_call_count_;
  }

 private:
  size_t on_download_removed_call_count_;
  size_t on_download_started_call_count_;
  size_t on_download_stopped_call_count_;
};

}  // anonymous namespace

namespace test {

class DownloadNotificationItemTest : public testing::Test {
 public:
  DownloadNotificationItemTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_),
        profile_(nullptr) {}

  void SetUp() override {
    testing::Test::SetUp();
    message_center::MessageCenter::Initialize();

    profile_manager_.reset(
        new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile("test-user");

    ui_manager_.reset(new StubNotificationUIManager);
    DownloadNotificationItem::SetStubNotificationUIManagerForTesting(
        ui_manager_.get());

    download_item_.reset(new NiceMock<content::MockDownloadItem>());
    ON_CALL(*download_item_, GetId()).WillByDefault(Return(12345));
    ON_CALL(*download_item_, GetState())
        .WillByDefault(Return(content::DownloadItem::IN_PROGRESS));
    ON_CALL(*download_item_, IsDangerous()).WillByDefault(Return(false));
    ON_CALL(*download_item_, GetFileNameToReportUser())
        .WillByDefault(Return(base::FilePath("TITLE.bin")));
    ON_CALL(*download_item_, GetDangerType())
        .WillByDefault(Return(content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS));
    ON_CALL(*download_item_, IsDone()).WillByDefault(Return(false));
  }

  void TearDown() override {
    download_notification_item_.reset();
    profile_manager_.reset();
    message_center::MessageCenter::Shutdown();
    testing::Test::TearDown();
  }

 protected:
  message_center::MessageCenter* message_center() {
    return message_center::MessageCenter::Get();
  }

  std::string notification_id() const {
    return download_notification_item_->notification_->id();
  }

  const Notification* notification() const {
    return ui_manager_->FindById(download_notification_item_->watcher_->id(),
                                 NotificationUIManager::GetProfileID(profile_));
  }

  size_t NotificationCount() const {
    return ui_manager_
        ->GetAllIdsByProfileAndSourceOrigin(
              NotificationUIManager::GetProfileID(profile_),
              GURL(DownloadNotificationItem::kDownloadNotificationOrigin))
        .size();
  }

  void RemoveNotification() {
    ui_manager_->CancelById(download_notification_item_->watcher_->id(),
                            NotificationUIManager::GetProfileID(profile_));

    // Waits, since removing a notification may cause an async job.
    base::RunLoop().RunUntilIdle();
  }

  // Trampoline methods to access a private method in DownloadNotificationItem.
  void NotificationItemClick() {
    return download_notification_item_->OnNotificationClick();
  }
  void NotificationItemButtonClick(int index) {
    return download_notification_item_->OnNotificationButtonClick(index);
  }

  bool ShownAsPopUp() {
    return !download_notification_item_->notification_->shown_as_popup();
  }

  void CreateDownloadNotificationItem() {
    download_notification_item_.reset(new DownloadNotificationItem(
        download_item_.get(), profile_, &delegate_));
  }

  base::MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;

  scoped_ptr<StubNotificationUIManager> ui_manager_;

  scoped_ptr<TestingProfileManager> profile_manager_;
  Profile* profile_;

  MockDownloadNotificationItemDelegate delegate_;
  scoped_ptr<NiceMock<content::MockDownloadItem>> download_item_;
  scoped_ptr<DownloadNotificationItem> download_notification_item_;
};

TEST_F(DownloadNotificationItemTest, ShowAndCloseNotification) {
  EXPECT_EQ(0u, NotificationCount());

  // Shows a notification
  CreateDownloadNotificationItem();
  download_item_->NotifyObserversDownloadOpened();

  // Confirms that the notification is shown as a popup.
  EXPECT_TRUE(ShownAsPopUp());
  EXPECT_EQ(1u, NotificationCount());

  // Makes sure the DownloadItem::Cancel() is not called.
  EXPECT_CALL(*download_item_, Cancel(_)).Times(0);
  // Closes it once.
  RemoveNotification();

  // Confirms that the notification is closed.
  EXPECT_EQ(0u, NotificationCount());

  // Makes sure the DownloadItem::Cancel() is never called.
  EXPECT_CALL(*download_item_, Cancel(_)).Times(0);
}

TEST_F(DownloadNotificationItemTest, PauseAndResumeNotification) {
  // Shows a notification
  CreateDownloadNotificationItem();
  download_item_->NotifyObserversDownloadOpened();

  // Confirms that the notification is shown as a popup.
  EXPECT_EQ(1u, NotificationCount());

  // Pauses and makes sure the DownloadItem::Pause() is called.
  EXPECT_CALL(*download_item_, Pause()).Times(1);
  EXPECT_CALL(*download_item_, IsPaused()).WillRepeatedly(Return(true));
  NotificationItemButtonClick(0);
  download_item_->NotifyObserversDownloadUpdated();

  // Resumes and makes sure the DownloadItem::Resume() is called.
  EXPECT_CALL(*download_item_, Resume()).Times(1);
  EXPECT_CALL(*download_item_, IsPaused()).WillRepeatedly(Return(false));
  NotificationItemButtonClick(0);
  download_item_->NotifyObserversDownloadUpdated();
}

TEST_F(DownloadNotificationItemTest, OpenDownload) {
  EXPECT_CALL(*download_item_, GetState())
      .WillRepeatedly(Return(content::DownloadItem::COMPLETE));
  EXPECT_CALL(*download_item_, IsDone()).WillRepeatedly(Return(true));

  // Shows a notification.
  CreateDownloadNotificationItem();
  download_item_->NotifyObserversDownloadOpened();
  download_item_->NotifyObserversDownloadUpdated();

  // Clicks and confirms that the OpenDownload() is called.
  EXPECT_CALL(*download_item_, OpenDownload()).Times(1);
  EXPECT_CALL(*download_item_, SetOpenWhenComplete(_)).Times(0);
  NotificationItemClick();
}

TEST_F(DownloadNotificationItemTest, OpenWhenComplete) {
  // Shows a notification
  CreateDownloadNotificationItem();
  download_item_->NotifyObserversDownloadOpened();

  EXPECT_CALL(*download_item_, OpenDownload()).Times(0);

  // Toggles open-when-complete (new value: true).
  EXPECT_CALL(*download_item_, SetOpenWhenComplete(true))
      .Times(1)
      .WillOnce(Return());
  NotificationItemClick();
  EXPECT_CALL(*download_item_, GetOpenWhenComplete())
      .WillRepeatedly(Return(true));

  // Toggles open-when-complete (new value: false).
  EXPECT_CALL(*download_item_, SetOpenWhenComplete(false))
      .Times(1)
      .WillOnce(Return());
  NotificationItemClick();
  EXPECT_CALL(*download_item_, GetOpenWhenComplete())
      .WillRepeatedly(Return(false));

  // Sets open-when-complete.
  EXPECT_CALL(*download_item_, SetOpenWhenComplete(true))
      .Times(1)
      .WillOnce(Return());
  NotificationItemClick();
  EXPECT_CALL(*download_item_, GetOpenWhenComplete())
      .WillRepeatedly(Return(true));

  // Downloading is completed.
  EXPECT_CALL(*download_item_, GetState())
      .WillRepeatedly(Return(content::DownloadItem::COMPLETE));
  EXPECT_CALL(*download_item_, IsDone()).WillRepeatedly(Return(true));
  download_item_->NotifyObserversDownloadUpdated();

  // DownloadItem::OpenDownload must not be called since the file opens
  // automatically due to the open-when-complete flag.
}

TEST_F(DownloadNotificationItemTest, DownloadNotificationItemDelegate) {
  // Shows a notification and checks OnDownloadStarted().
  EXPECT_EQ(0u, delegate_.GetOnDownloadStartedCallCount());
  CreateDownloadNotificationItem();
  EXPECT_EQ(1u, delegate_.GetOnDownloadStartedCallCount());

  download_item_->NotifyObserversDownloadOpened();
  download_item_->NotifyObserversDownloadUpdated();

  // Checks OnDownloadStopped().
  EXPECT_EQ(0u, delegate_.GetOnDownloadStoppedCallCount());
  EXPECT_CALL(*download_item_, GetState())
      .WillRepeatedly(Return(content::DownloadItem::COMPLETE));
  EXPECT_CALL(*download_item_, IsDone()).WillRepeatedly(Return(true));
  download_item_->NotifyObserversDownloadUpdated();
  EXPECT_EQ(1u, delegate_.GetOnDownloadStoppedCallCount());

  // Checks OnDownloadRemoved().
  EXPECT_EQ(0u, delegate_.GetOnDownloadRemovedCallCount());
  download_item_->NotifyObserversDownloadRemoved();
  EXPECT_EQ(1u, delegate_.GetOnDownloadRemovedCallCount());

  download_item_.reset();
}

}  // namespace test
