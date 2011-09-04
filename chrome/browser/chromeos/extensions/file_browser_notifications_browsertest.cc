// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <string>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/chromeos/extensions/file_browser_notifications.h"
#include "chrome/browser/chromeos/notifications/balloon_collection_impl.h"
#include "chrome/browser/notifications/balloon.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::StrEq;

namespace chromeos {

class MockNotificationUI : public BalloonCollectionImpl::NotificationUI {
 public:
  virtual ~MockNotificationUI() {}

  MOCK_METHOD1(Add, void(Balloon* balloon));
  MOCK_METHOD1(Update, bool(Balloon* balloon));
  MOCK_METHOD1(Remove, void(Balloon* balloon));
  MOCK_METHOD1(Show, void(Balloon* balloon));

  virtual void ResizeNotification(Balloon* balloon, const gfx::Size& size)
      OVERRIDE {
  }
  virtual void SetActiveView(BalloonViewImpl* view) OVERRIDE {}
};

class MockFileBrowserNotifications : public FileBrowserNotifications {
 public:
  explicit MockFileBrowserNotifications(Profile* profile)
      : FileBrowserNotifications(profile) {
  }
  virtual ~MockFileBrowserNotifications() {}

  virtual void PostDelayedShowNotificationTask(
      const std::string& notification_id,
      NotificationType type,
      const string16&  message,
      size_t delay_ms) {
    show_callback_data_.id = notification_id;
    show_callback_data_.type = type;
    show_callback_data_.message = message;
  }

  virtual void PostDelayedHideNotificationTask(NotificationType type,
                                               const std::string  path,
                                               size_t delay_ms) {
    hide_callback_data_.type = type;
    hide_callback_data_.path = path;
  }

  void ExecuteShow() {
    FileBrowserNotifications::ShowNotificationDelayedTask(
      show_callback_data_.id, show_callback_data_.type,
      show_callback_data_.message, AsWeakPtr());
  }

  void ExecuteHide() {
    FileBrowserNotifications::HideNotificationDelayedTask(
      hide_callback_data_.type, hide_callback_data_.path, AsWeakPtr());
  }

 private:
  struct ShowCallbackData {
    std::string id;
    NotificationType type;
    string16 message;
  };

  ShowCallbackData show_callback_data_;

  struct HideCallbackData {
    NotificationType type;
    std::string path;
  };

  HideCallbackData hide_callback_data_;
};

class FileBrowserNotificationsTest : public InProcessBrowserTest {
 public:
  FileBrowserNotificationsTest() {}

 protected:
  void ChangeNotificationUIMock() {
    // collection will take ownership of the mock.
    mock_notification_ui_ = new MockNotificationUI();
    collection_->set_notification_ui(mock_notification_ui_);
  }

  void InitNotificationUIMock() {
    collection_ = static_cast<BalloonCollectionImpl*>(
        g_browser_process->notification_ui_manager()->balloon_collection());
    ChangeNotificationUIMock();
  }

  bool FindNotification(const std::string& id) {
    return notifications_->notifications().find(id) !=
           notifications_->notifications().end();
  }

  BalloonCollectionImpl* collection_;
  MockNotificationUI* mock_notification_ui_;
  scoped_ptr<FileBrowserNotifications> notifications_;
};

MATCHER_P(BalloonNotificationMatcher, expected_id, "") {
  return arg->notification().notification_id() == expected_id;
}

IN_PROC_BROWSER_TEST_F(FileBrowserNotificationsTest, TestBasic) {
  InitNotificationUIMock();
  notifications_.reset(new MockFileBrowserNotifications(browser()->profile()));

  EXPECT_CALL(*mock_notification_ui_,
              Add(BalloonNotificationMatcher("Dpath")));
  notifications_->ShowNotification(FileBrowserNotifications::DEVICE, "path");

  EXPECT_CALL(*mock_notification_ui_,
              Update(BalloonNotificationMatcher("Dpath")))
      .WillOnce(Return(true));
  notifications_->ShowNotification(FileBrowserNotifications::DEVICE, "path");

  EXPECT_EQ(1u, notifications_->notifications().size());
  EXPECT_TRUE(FindNotification("Dpath"));

  EXPECT_CALL(*mock_notification_ui_,
              Add(BalloonNotificationMatcher("DFpath")));
  notifications_->ShowNotification(FileBrowserNotifications::DEVICE_FAIL,
                                   "path");
  EXPECT_EQ(2u, notifications_->notifications().size());
  EXPECT_TRUE(FindNotification("DFpath"));

  EXPECT_CALL(*mock_notification_ui_,
              Remove(BalloonNotificationMatcher("DFpath")));
  notifications_->HideNotification(FileBrowserNotifications::DEVICE_FAIL,
                                   "path");

  EXPECT_EQ(1u, notifications_->notifications().size());
  EXPECT_FALSE(FindNotification("DFpath"));

  ui_test_utils::RunAllPendingInMessageLoop();

  ChangeNotificationUIMock();

  EXPECT_CALL(*mock_notification_ui_, Remove(_))
      .Times(1);
};

IN_PROC_BROWSER_TEST_F(FileBrowserNotificationsTest, ShowDelayedTest) {
  InitNotificationUIMock();
  MockFileBrowserNotifications* mocked_notifications =
      new MockFileBrowserNotifications(browser()->profile());
  notifications_.reset(mocked_notifications);

  EXPECT_CALL(*mock_notification_ui_,
              Add(BalloonNotificationMatcher("Dpath")));
  notifications_->ShowNotificationDelayed(FileBrowserNotifications::DEVICE,
                                          "path", 3000);
  mocked_notifications->ExecuteShow();

  EXPECT_CALL(*mock_notification_ui_,
              Add(BalloonNotificationMatcher("DFpath")));
  notifications_->ShowNotificationDelayed(FileBrowserNotifications::DEVICE_FAIL,
                                          "path", 3000);
  notifications_->ShowNotification(FileBrowserNotifications::DEVICE_FAIL,
                                   "path");

  ChangeNotificationUIMock();
  EXPECT_CALL(*mock_notification_ui_,
              Update(BalloonNotificationMatcher("DFpath")));
  mocked_notifications->ExecuteShow();

  EXPECT_CALL(*mock_notification_ui_,
              Remove(BalloonNotificationMatcher("Fpath")))
      .Times(0);
  EXPECT_CALL(*mock_notification_ui_,
              Add(BalloonNotificationMatcher("Fpath")))
      .Times(0);
  notifications_->ShowNotificationDelayed(
      FileBrowserNotifications::FORMAT_FAIL, "path", 3000);
  notifications_->HideNotification(FileBrowserNotifications::FORMAT_FAIL,
                                   "path");
  ui_test_utils::RunAllPendingInMessageLoop();

  mocked_notifications->ExecuteShow();

  ChangeNotificationUIMock();
  EXPECT_CALL(*mock_notification_ui_, Remove(_))
      .Times(2);

  ui_test_utils::RunAllPendingInMessageLoop();
}

IN_PROC_BROWSER_TEST_F(FileBrowserNotificationsTest, HideDelayedTest) {
  InitNotificationUIMock();
  MockFileBrowserNotifications* mocked_notifications =
      new MockFileBrowserNotifications(browser()->profile());
  notifications_.reset(mocked_notifications);

  EXPECT_CALL(*mock_notification_ui_,
              Add(BalloonNotificationMatcher("Dpath")));
  notifications_->ShowNotification(FileBrowserNotifications::DEVICE, "path");

  notifications_->HideNotificationDelayed(FileBrowserNotifications::DEVICE,
                                          "path", 3000);
  ChangeNotificationUIMock();
  EXPECT_CALL(*mock_notification_ui_,
              Remove(BalloonNotificationMatcher("Dpath")));
  mocked_notifications->ExecuteHide();

  ui_test_utils::RunAllPendingInMessageLoop();

  EXPECT_CALL(*mock_notification_ui_,
              Add(BalloonNotificationMatcher("DFpath")));
  EXPECT_CALL(*mock_notification_ui_,
              Remove(BalloonNotificationMatcher("DFpath")));
  notifications_->ShowNotification(FileBrowserNotifications::DEVICE_FAIL,
                                   "path");
  notifications_->HideNotification(FileBrowserNotifications::DEVICE_FAIL,
                                   "path");
  ui_test_utils::RunAllPendingInMessageLoop();

  notifications_->HideNotificationDelayed(FileBrowserNotifications::DEVICE_FAIL,
                                          "path", 3000);
  ChangeNotificationUIMock();
  EXPECT_CALL(*mock_notification_ui_,
              Remove(BalloonNotificationMatcher("DFpath")))
      .Times(0);
  mocked_notifications->ExecuteHide();

  ui_test_utils::RunAllPendingInMessageLoop();
}

}  // namespace chromeos.

