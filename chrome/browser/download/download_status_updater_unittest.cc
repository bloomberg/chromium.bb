// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "chrome/browser/download/download_status_updater.h"
#include "content/public/test/mock_download_item.h"
#include "content/public/test/mock_download_manager.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::AtLeast;
using testing::Invoke;
using testing::Mock;
using testing::Return;
using testing::SetArgPointee;
using testing::StrictMock;
using testing::WithArg;
using testing::_;

class TestDownloadStatusUpdater : public DownloadStatusUpdater {
 public:
  TestDownloadStatusUpdater() : notification_count_(0),
                                acceptable_notification_item_(NULL) {
  }
  void SetAcceptableNotificationItem(content::DownloadItem* item) {
    acceptable_notification_item_ = item;
  }
  size_t NotificationCount() {
    return notification_count_;
  }
 protected:
  virtual void UpdateAppIconDownloadProgress(
      content::DownloadItem* download) OVERRIDE {
    ++notification_count_;
    if (acceptable_notification_item_)
      EXPECT_EQ(acceptable_notification_item_, download);
  }
 private:
  size_t notification_count_;
  content::DownloadItem* acceptable_notification_item_;
};

class DownloadStatusUpdaterTest : public testing::Test {
 public:
  DownloadStatusUpdaterTest()
      : updater_(new TestDownloadStatusUpdater()),
        ui_thread_(content::BrowserThread::UI, &loop_) {}

  virtual ~DownloadStatusUpdaterTest() {
    for (size_t mgr_idx = 0; mgr_idx < managers_.size(); ++mgr_idx) {
      EXPECT_CALL(*Manager(mgr_idx), RemoveObserver(_));
    }

    delete updater_;
    updater_ = NULL;
    VerifyAndClearExpectations();

    managers_.clear();
    for (std::vector<Items>::iterator it = manager_items_.begin();
         it != manager_items_.end(); ++it)
      STLDeleteContainerPointers(it->begin(), it->end());

    loop_.RunUntilIdle();  // Allow DownloadManager destruction.
  }

 protected:
  // Attach some number of DownloadManagers to the updater.
  void SetupManagers(int manager_count) {
    DCHECK_EQ(0U, managers_.size());
    for (int i = 0; i < manager_count; ++i) {
      content::MockDownloadManager* mgr =
          new StrictMock<content::MockDownloadManager>;
      managers_.push_back(mgr);
    }
  }

  void SetObserver(content::DownloadManager::Observer* observer) {
    manager_observers_[manager_observer_index_] = observer;
  }

  // Hook the specified manager into the updater.
  void LinkManager(int i) {
    content::MockDownloadManager* mgr = managers_[i];
    manager_observer_index_ = i;
    while (manager_observers_.size() <= static_cast<size_t>(i)) {
      manager_observers_.push_back(NULL);
    }
    EXPECT_CALL(*mgr, AddObserver(_))
        .WillOnce(WithArg<0>(Invoke(
            this, &DownloadStatusUpdaterTest::SetObserver)));
    updater_->AddManager(mgr);
  }

  // Add some number of Download items to a particular manager.
  void AddItems(int manager_index, int item_count, int in_progress_count) {
    DCHECK_GT(managers_.size(), static_cast<size_t>(manager_index));
    content::MockDownloadManager* manager = managers_[manager_index];

    if (manager_items_.size() <= static_cast<size_t>(manager_index))
      manager_items_.resize(manager_index+1);

    std::vector<content::DownloadItem*> item_list;
    for (int i = 0; i < item_count; ++i) {
      content::MockDownloadItem* item =
          new StrictMock<content::MockDownloadItem>;
      content::DownloadItem::DownloadState state =
          i < in_progress_count ? content::DownloadItem::IN_PROGRESS
              : content::DownloadItem::CANCELLED;
      EXPECT_CALL(*item, GetState()).WillRepeatedly(Return(state));
      manager_items_[manager_index].push_back(item);
    }
    EXPECT_CALL(*manager, GetAllDownloads(_))
        .WillRepeatedly(SetArgPointee<0>(manager_items_[manager_index]));
  }

  // Return the specified manager.
  content::MockDownloadManager* Manager(int manager_index) {
    DCHECK_GT(managers_.size(), static_cast<size_t>(manager_index));
    return managers_[manager_index];
  }

  // Return the specified item.
  content::MockDownloadItem* Item(int manager_index, int item_index) {
    DCHECK_GT(manager_items_.size(), static_cast<size_t>(manager_index));
    DCHECK_GT(manager_items_[manager_index].size(),
              static_cast<size_t>(item_index));
    // All DownloadItems in manager_items_ are MockDownloadItems.
    return static_cast<content::MockDownloadItem*>(
        manager_items_[manager_index][item_index]);
  }

  // Set return values relevant to |DownloadStatusUpdater::GetProgress()|
  // for the specified item.
  void SetItemValues(int manager_index, int item_index,
                     int received_bytes, int total_bytes, bool notify) {
    content::MockDownloadItem* item(Item(manager_index, item_index));
    EXPECT_CALL(*item, GetReceivedBytes())
        .WillRepeatedly(Return(received_bytes));
    EXPECT_CALL(*item, GetTotalBytes())
        .WillRepeatedly(Return(total_bytes));
    if (notify)
      updater_->OnDownloadUpdated(managers_[manager_index], item);
  }

  // Transition specified item to completed.
  void CompleteItem(int manager_index, int item_index) {
    content::MockDownloadItem* item(Item(manager_index, item_index));
    EXPECT_CALL(*item, GetState())
        .WillRepeatedly(Return(content::DownloadItem::COMPLETE));
    updater_->OnDownloadUpdated(managers_[manager_index], item);
  }

  // Verify and clear all mocks expectations.
  void VerifyAndClearExpectations() {
    for (ScopedVector<content::MockDownloadManager>::iterator it
             = managers_.begin(); it != managers_.end(); ++it)
      Mock::VerifyAndClearExpectations(*it);
    for (std::vector<Items>::iterator it = manager_items_.begin();
         it != manager_items_.end(); ++it)
      for (Items::iterator sit = it->begin(); sit != it->end(); ++sit)
        Mock::VerifyAndClearExpectations(*sit);
  }

  ScopedVector<content::MockDownloadManager> managers_;
  // DownloadItem so that it can be assigned to the result of SearchDownloads.
  typedef std::vector<content::DownloadItem*> Items;
  std::vector<Items> manager_items_;
  int manager_observer_index_;

  std::vector<content::DownloadManager::Observer*> manager_observers_;

  // Pointer so we can verify that destruction triggers appropriate
  // changes.
  TestDownloadStatusUpdater *updater_;

  // Thread so that the DownloadManager (which is a DeleteOnUIThread
  // object) can be deleted.
  // TODO(rdsmith): This can be removed when the DownloadManager
  // is no longer required to be deleted on the UI thread.
  base::MessageLoop loop_;
  content::TestBrowserThread ui_thread_;
};

// Test null updater.
TEST_F(DownloadStatusUpdaterTest, Basic) {
  float progress = -1;
  int download_count = -1;
  EXPECT_TRUE(updater_->GetProgress(&progress, &download_count));
  EXPECT_FLOAT_EQ(0.0f, progress);
  EXPECT_EQ(0, download_count);
}

// Test updater with null manager.
TEST_F(DownloadStatusUpdaterTest, OneManagerNoItems) {
  SetupManagers(1);
  AddItems(0, 0, 0);
  LinkManager(0);
  VerifyAndClearExpectations();

  float progress = -1;
  int download_count = -1;
  EXPECT_CALL(*managers_[0], GetAllDownloads(_))
      .WillRepeatedly(SetArgPointee<0>(manager_items_[0]));
  EXPECT_TRUE(updater_->GetProgress(&progress, &download_count));
  EXPECT_FLOAT_EQ(0.0f, progress);
  EXPECT_EQ(0, download_count);
}

// Test updater with non-null manager, including transition an item to
// |content::DownloadItem::COMPLETE| and adding a new item.
TEST_F(DownloadStatusUpdaterTest, OneManagerManyItems) {
  SetupManagers(1);
  AddItems(0, 3, 2);
  LinkManager(0);

  // Prime items
  SetItemValues(0, 0, 10, 20, false);
  SetItemValues(0, 1, 50, 60, false);
  SetItemValues(0, 2, 90, 90, false);

  float progress = -1;
  int download_count = -1;
  EXPECT_TRUE(updater_->GetProgress(&progress, &download_count));
  EXPECT_FLOAT_EQ((10+50)/(20.0f+60), progress);
  EXPECT_EQ(2, download_count);

  // Transition one item to completed and confirm progress is updated
  // properly.
  CompleteItem(0, 0);
  EXPECT_TRUE(updater_->GetProgress(&progress, &download_count));
  EXPECT_FLOAT_EQ(50/60.0f, progress);
  EXPECT_EQ(1, download_count);

  // Add a new item to manager and confirm progress is updated properly.
  AddItems(0, 1, 1);
  SetItemValues(0, 3, 150, 200, false);
  manager_observers_[0]->OnDownloadCreated(
      managers_[0], manager_items_[0][manager_items_[0].size()-1]);

  EXPECT_TRUE(updater_->GetProgress(&progress, &download_count));
  EXPECT_FLOAT_EQ((50+150)/(60+200.0f), progress);
  EXPECT_EQ(2, download_count);
}

// Test to ensure that the download progress notification is called correctly.
TEST_F(DownloadStatusUpdaterTest, ProgressNotification) {
  size_t expected_notifications = updater_->NotificationCount();
  SetupManagers(1);
  AddItems(0, 2, 2);
  LinkManager(0);

  // Expect two notifications, one for each item; which item will come first
  // isn't defined so it cannot be tested.
  expected_notifications += 2;
  ASSERT_EQ(expected_notifications, updater_->NotificationCount());

  // Make progress on the first item.
  updater_->SetAcceptableNotificationItem(Item(0, 0));
  SetItemValues(0, 0, 10, 20, true);
  ++expected_notifications;
  ASSERT_EQ(expected_notifications, updater_->NotificationCount());

  // Second item completes!
  updater_->SetAcceptableNotificationItem(Item(0, 1));
  CompleteItem(0, 1);
  ++expected_notifications;
  ASSERT_EQ(expected_notifications, updater_->NotificationCount());

  // First item completes.
  updater_->SetAcceptableNotificationItem(Item(0, 0));
  CompleteItem(0, 0);
  ++expected_notifications;
  ASSERT_EQ(expected_notifications, updater_->NotificationCount());

  updater_->SetAcceptableNotificationItem(NULL);
}

// Confirm we recognize the situation where we have an unknown size.
TEST_F(DownloadStatusUpdaterTest, UnknownSize) {
  SetupManagers(1);
  AddItems(0, 2, 2);
  LinkManager(0);

  // Prime items
  SetItemValues(0, 0, 10, 20, false);
  SetItemValues(0, 1, 50, -1, false);

  float progress = -1;
  int download_count = -1;
  EXPECT_FALSE(updater_->GetProgress(&progress, &download_count));
}

// Test many null managers.
TEST_F(DownloadStatusUpdaterTest, ManyManagersNoItems) {
  SetupManagers(1);
  AddItems(0, 0, 0);
  LinkManager(0);

  float progress = -1;
  int download_count = -1;
  EXPECT_TRUE(updater_->GetProgress(&progress, &download_count));
  EXPECT_FLOAT_EQ(0.0f, progress);
  EXPECT_EQ(0, download_count);
}

// Test many managers with all items complete.
TEST_F(DownloadStatusUpdaterTest, ManyManagersEmptyItems) {
  SetupManagers(2);
  AddItems(0, 3, 0);
  LinkManager(0);
  AddItems(1, 3, 0);
  LinkManager(1);

  float progress = -1;
  int download_count = -1;
  EXPECT_TRUE(updater_->GetProgress(&progress, &download_count));
  EXPECT_FLOAT_EQ(0.0f, progress);
  EXPECT_EQ(0, download_count);
}

// Test many managers with some non-complete items.
TEST_F(DownloadStatusUpdaterTest, ManyManagersMixedItems) {
  SetupManagers(2);
  AddItems(0, 3, 2);
  LinkManager(0);
  AddItems(1, 3, 1);
  LinkManager(1);

  SetItemValues(0, 0, 10, 20, false);
  SetItemValues(0, 1, 50, 60, false);
  SetItemValues(1, 0, 80, 90, false);

  float progress = -1;
  int download_count = -1;
  EXPECT_TRUE(updater_->GetProgress(&progress, &download_count));
  EXPECT_FLOAT_EQ((10+50+80)/(20.0f+60+90), progress);
  EXPECT_EQ(3, download_count);
}
