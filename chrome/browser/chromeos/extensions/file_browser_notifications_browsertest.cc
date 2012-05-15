// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_browser_notifications.h"

#include <gtest/gtest.h>
#include <string>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/notifications/balloon.h"
#include "chrome/browser/notifications/balloon_collection.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"

namespace chromeos {

class MockFileBrowserNotifications : public FileBrowserNotifications {
 public:
  explicit MockFileBrowserNotifications(Profile* profile)
      : FileBrowserNotifications(profile) {
  }
  virtual ~MockFileBrowserNotifications() {}

  // Records the notification so we can force it to show later.
  virtual void PostDelayedShowNotificationTask(
      const std::string& notification_id,
      NotificationType type,
      const string16&  message,
      size_t delay_ms) OVERRIDE {
    show_callback_data_.id = notification_id;
    show_callback_data_.type = type;
    show_callback_data_.message = message;
  }

  // Records the notification so we can force it to hide later.
  virtual void PostDelayedHideNotificationTask(NotificationType type,
                                               const std::string  path,
                                               size_t delay_ms) OVERRIDE {
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
  FileBrowserNotificationsTest() : collection_(NULL) {}

  virtual void CleanUpOnMainThread() OVERRIDE {
    notifications_.reset();
  }

 protected:
  // This must be initialized late in test startup.
  void InitNotifications() {
    Profile* profile = browser()->profile();
    notifications_.reset(new MockFileBrowserNotifications(profile));
    collection_ =
        g_browser_process->notification_ui_manager()->balloon_collection();
  }

  bool FindNotification(const std::string& id) {
    return notifications_->notifications().find(id) !=
           notifications_->notifications().end();
  }

  bool FindBalloon(const std::string& id) {
    const std::deque<Balloon*>& balloons = collection_->GetActiveBalloons();
    for (std::deque<Balloon*>::const_iterator it = balloons.begin();
         it != balloons.end();
         ++it) {
      Balloon* balloon = *it;
      if (balloon->notification().notification_id() == id)
        return true;
    }
    return false;
  }

  BalloonCollection* collection_;
  scoped_ptr<MockFileBrowserNotifications> notifications_;
};

IN_PROC_BROWSER_TEST_F(FileBrowserNotificationsTest, TestBasic) {
  InitNotifications();
  // We start with no balloons.
  EXPECT_EQ(0u, collection_->GetActiveBalloons().size());

  // Showing a notification both updates our data and shows a balloon.
  notifications_->ShowNotification(FileBrowserNotifications::DEVICE, "path");
  EXPECT_EQ(1u, notifications_->notifications().size());
  EXPECT_TRUE(FindNotification("Dpath"));
  EXPECT_EQ(1u, collection_->GetActiveBalloons().size());
  EXPECT_TRUE(FindBalloon("Dpath"));

  // Updating the same notification maintains the same balloon.
  notifications_->ShowNotification(FileBrowserNotifications::DEVICE, "path");
  EXPECT_EQ(1u, notifications_->notifications().size());
  EXPECT_TRUE(FindNotification("Dpath"));
  EXPECT_EQ(1u, collection_->GetActiveBalloons().size());
  EXPECT_TRUE(FindBalloon("Dpath"));

  // A new notification adds a new balloon.
  notifications_->ShowNotification(FileBrowserNotifications::DEVICE_FAIL,
                                   "path");
  EXPECT_EQ(2u, notifications_->notifications().size());
  EXPECT_TRUE(FindNotification("DFpath"));
  EXPECT_TRUE(FindNotification("Dpath"));
  EXPECT_EQ(2u, collection_->GetActiveBalloons().size());
  EXPECT_TRUE(FindBalloon("DFpath"));
  EXPECT_TRUE(FindBalloon("Dpath"));

  // Hiding a notification removes it from our data.
  notifications_->HideNotification(FileBrowserNotifications::DEVICE_FAIL,
                                   "path");
  EXPECT_EQ(1u, notifications_->notifications().size());
  EXPECT_FALSE(FindNotification("DFpath"));
  EXPECT_TRUE(FindNotification("Dpath"));

  // Balloons don't go away until we run the message loop.
  EXPECT_EQ(2u, collection_->GetActiveBalloons().size());
  EXPECT_TRUE(FindBalloon("DFpath"));
  EXPECT_TRUE(FindBalloon("Dpath"));

  // Running the message loop allows the balloon to disappear.
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_EQ(1u, collection_->GetActiveBalloons().size());
  EXPECT_FALSE(FindBalloon("DFpath"));
  EXPECT_TRUE(FindBalloon("Dpath"));
};

// TODO(jamescook): This test is flaky on linux_chromeos, occasionally causing
// this assertion failure inside Gtk:
//   "murrine_style_draw_box: assertion `height >= -1' failed"
// There may be an underlying bug in the ChromeOS notification code.
// I'm not marking it as FLAKY because this doesn't happen on the bots.
#define MAYBE_ShowDelayedTest ShowDelayedTest
IN_PROC_BROWSER_TEST_F(FileBrowserNotificationsTest, ShowDelayedTest) {
  InitNotifications();
  // Adding a delayed notification does not show a balloon.
  notifications_->ShowNotificationDelayed(FileBrowserNotifications::DEVICE,
                                          "path", 3000);
  EXPECT_EQ(0u, collection_->GetActiveBalloons().size());

  // Forcing the show to happen makes the balloon appear.
  notifications_->ExecuteShow();
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_EQ(1u, collection_->GetActiveBalloons().size());
  EXPECT_TRUE(FindBalloon("Dpath"));

  // Showing a notification both immediately and delayed results in one
  // additional balloon.
  notifications_->ShowNotificationDelayed(FileBrowserNotifications::DEVICE_FAIL,
                                          "path", 3000);
  notifications_->ShowNotification(FileBrowserNotifications::DEVICE_FAIL,
                                   "path");
  EXPECT_EQ(2u, collection_->GetActiveBalloons().size());
  EXPECT_TRUE(FindBalloon("Dpath"));
  EXPECT_TRUE(FindBalloon("DFpath"));

  // When the delayed notification arrives, it's an update, so we still only
  // have two balloons.
  notifications_->ExecuteShow();
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_EQ(2u, collection_->GetActiveBalloons().size());
  EXPECT_TRUE(FindBalloon("Dpath"));
  EXPECT_TRUE(FindBalloon("DFpath"));

  // If we schedule a show for later, then hide before it becomes visible,
  // the balloon should not be added.
  notifications_->ShowNotificationDelayed(
      FileBrowserNotifications::FORMAT_FAIL, "path", 3000);
  notifications_->HideNotification(FileBrowserNotifications::FORMAT_FAIL,
                                   "path");
  EXPECT_EQ(2u, collection_->GetActiveBalloons().size());
  EXPECT_TRUE(FindBalloon("Dpath"));
  EXPECT_TRUE(FindBalloon("DFpath"));
  EXPECT_FALSE(FindBalloon("Fpath"));

  // Even when we try to force the show, nothing appears, because the balloon
  // was explicitly hidden.
  notifications_->ExecuteShow();
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_EQ(2u, collection_->GetActiveBalloons().size());
  EXPECT_TRUE(FindBalloon("Dpath"));
  EXPECT_TRUE(FindBalloon("DFpath"));
  EXPECT_FALSE(FindBalloon("Fpath"));
}

IN_PROC_BROWSER_TEST_F(FileBrowserNotificationsTest, HideDelayedTest) {
  InitNotifications();
  // Showing now, and scheduling a hide for later, results in one balloon.
  notifications_->ShowNotification(FileBrowserNotifications::DEVICE, "path");
  notifications_->HideNotificationDelayed(FileBrowserNotifications::DEVICE,
                                          "path", 3000);
  EXPECT_EQ(1u, collection_->GetActiveBalloons().size());
  EXPECT_TRUE(FindBalloon("Dpath"));

  // Forcing the hide removes the balloon.
  notifications_->ExecuteHide();
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_EQ(0u, collection_->GetActiveBalloons().size());

  // Immediate show then hide results in no balloons.
  notifications_->ShowNotification(FileBrowserNotifications::DEVICE_FAIL,
                                   "path");
  notifications_->HideNotification(FileBrowserNotifications::DEVICE_FAIL,
                                   "path");
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_EQ(0u, collection_->GetActiveBalloons().size());

  // Delayed hide for a notification that doesn't exist does nothing.
  notifications_->HideNotificationDelayed(FileBrowserNotifications::DEVICE_FAIL,
                                          "path", 3000);
  notifications_->ExecuteHide();
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_EQ(0u, collection_->GetActiveBalloons().size());
}

}  // namespace chromeos.
