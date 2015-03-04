// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/notification/download_notification_item.h"

#include "base/run_loop.h"
#include "base/test/test_simple_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "content/public/test/mock_download_item.h"
#include "content/public/test/mock_download_manager.h"
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
      : runner_(new base::TestSimpleTaskRunner), runner_handler_(runner_) {}

  void SetUp() override {
    testing::Test::SetUp();
    message_center::MessageCenter::Initialize();

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
    message_center::MessageCenter::Shutdown();
    testing::Test::TearDown();
  }

 protected:
  message_center::MessageCenter* message_center() {
    return message_center::MessageCenter::Get();
  }

  std::string notification_id() {
    return download_notification_item_->notification_->id();
  }

  message_center::Notification* notification() {
    return message_center()->FindVisibleNotificationById(notification_id());
  }

  // Trampoline methods to access a private method in DownloadNotificationItem.
  void NotificationItemClick() {
    return download_notification_item_->OnNotificationClick();
  }
  void NotificationItemButtonClick(int index) {
    return download_notification_item_->OnNotificationButtonClick(index);
  }

  bool IsPopupNotification(const std::string& notification_id) {
    message_center::NotificationList::PopupNotifications popups =
        message_center()->GetPopupNotifications();
    for (auto it = popups.begin(); it != popups.end(); it++) {
      if ((*it)->id() == notification_id) {
        return true;
      }
    }
    return false;
  }

  void CreateDownloadNotificationItem() {
    download_notification_item_.reset(
        new DownloadNotificationItem(download_item_.get(), &delegate_));
  }

  scoped_refptr<base::TestSimpleTaskRunner> runner_;
  base::ThreadTaskRunnerHandle runner_handler_;

  MockDownloadNotificationItemDelegate delegate_;
  scoped_ptr<NiceMock<content::MockDownloadItem>> download_item_;
  scoped_ptr<DownloadNotificationItem> download_notification_item_;
};

TEST_F(DownloadNotificationItemTest, ShowAndCloseNotification) {
  EXPECT_EQ(0u, message_center()->NotificationCount());

  // Shows a notification
  CreateDownloadNotificationItem();
  download_item_->NotifyObserversDownloadOpened();

  // Confirms that the notification is shown as a popup.
  EXPECT_EQ(1u, message_center()->NotificationCount());
  EXPECT_TRUE(IsPopupNotification(notification_id()));

  // Makes sure the DownloadItem::Cancel() is not called.
  EXPECT_CALL(*download_item_, Cancel(_)).Times(0);
  // Closes it once.
  message_center()->RemoveNotification(notification_id(), true);

  // Confirms that the notification is shown but is not a popup.
  EXPECT_EQ(1u, message_center()->NotificationCount());
  EXPECT_FALSE(IsPopupNotification(notification_id()));

  // Makes sure the DownloadItem::Cancel() is called once.
  EXPECT_CALL(*download_item_, Cancel(_)).Times(1);
  // Closes it again.
  message_center()->RemoveNotification(notification_id(), true);

  // Confirms that the notification is closed.
  EXPECT_EQ(0u, message_center()->NotificationCount());
}

TEST_F(DownloadNotificationItemTest, PauseAndResumeNotification) {
  // Shows a notification
  CreateDownloadNotificationItem();
  download_item_->NotifyObserversDownloadOpened();

  // Confirms that the notification is shown as a popup.
  EXPECT_EQ(message_center()->NotificationCount(), 1u);
  EXPECT_TRUE(IsPopupNotification(notification_id()));

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
