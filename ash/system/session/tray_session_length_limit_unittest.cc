// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/session/tray_session_length_limit.h"

#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/system/tray/label_tray_view.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_test_api.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_types.h"

namespace ash {

class TraySessionLengthLimitTest : public AshTestBase {
 public:
  TraySessionLengthLimitTest() = default;
  ~TraySessionLengthLimitTest() override = default;

 protected:
  LabelTrayView* GetSessionLengthLimitTrayView() {
    return SystemTrayTestApi(GetPrimarySystemTray())
        .tray_session_length_limit()
        ->tray_bubble_view_;
  }

  void UpdateSessionLengthLimitInMin(int mins) {
    Shell::Get()->session_controller()->SetSessionLengthLimit(
        base::TimeDelta::FromMinutes(mins), base::TimeTicks::Now());
  }

  message_center::Notification* GetNotification() {
    const message_center::NotificationList::Notifications& notifications =
        message_center::MessageCenter::Get()->GetVisibleNotifications();
    for (message_center::NotificationList::Notifications::const_iterator iter =
             notifications.begin();
         iter != notifications.end(); ++iter) {
      if ((*iter)->id() == TraySessionLengthLimit::kNotificationId)
        return *iter;
    }
    return nullptr;
  }

  void ClearSessionLengthLimit() {
    Shell::Get()->session_controller()->SetSessionLengthLimit(
        base::TimeDelta(), base::TimeTicks());
  }

  void RemoveNotification() {
    message_center::MessageCenter::Get()->RemoveNotification(
        TraySessionLengthLimit::kNotificationId, false /* by_user */);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TraySessionLengthLimitTest);
};

TEST_F(TraySessionLengthLimitTest, Visibility) {
  SystemTray* system_tray = GetPrimarySystemTray();

  // By default there is no session length limit item.
  system_tray->ShowDefaultView(BUBBLE_CREATE_NEW);
  EXPECT_FALSE(GetSessionLengthLimitTrayView());
  system_tray->CloseBubble();

  // Setting a length limit shows an item in the system tray menu.
  UpdateSessionLengthLimitInMin(10);
  system_tray->ShowDefaultView(BUBBLE_CREATE_NEW);
  ASSERT_TRUE(GetSessionLengthLimitTrayView());
  EXPECT_TRUE(GetSessionLengthLimitTrayView()->visible());
  system_tray->CloseBubble();

  // Removing the session length limit removes the tray menu item.
  UpdateSessionLengthLimitInMin(0);
  system_tray->ShowDefaultView(BUBBLE_CREATE_NEW);
  EXPECT_FALSE(GetSessionLengthLimitTrayView());
  system_tray->CloseBubble();
}

TEST_F(TraySessionLengthLimitTest, Notification) {
  // No notifications when no session limit.
  EXPECT_FALSE(GetNotification());

  // Limit is 15 min.
  UpdateSessionLengthLimitInMin(15);
  message_center::Notification* notification = GetNotification();
  EXPECT_TRUE(notification);
  EXPECT_EQ(message_center::SYSTEM_PRIORITY, notification->priority());
  base::string16 first_content = notification->message();
  // Should read the content.
  EXPECT_TRUE(notification->rich_notification_data()
                  .should_make_spoken_feedback_for_popup_updates);

  // Limit is 10 min.
  UpdateSessionLengthLimitInMin(10);
  notification = GetNotification();
  EXPECT_TRUE(notification);
  EXPECT_EQ(message_center::SYSTEM_PRIORITY, notification->priority());
  // The content should be updated.
  EXPECT_NE(first_content, notification->message());
  // Should NOT read, because just update the remaining time.
  EXPECT_FALSE(notification->rich_notification_data()
                   .should_make_spoken_feedback_for_popup_updates);

  // Limit is 3 min.
  UpdateSessionLengthLimitInMin(3);
  notification = GetNotification();
  EXPECT_TRUE(notification);
  EXPECT_EQ(message_center::SYSTEM_PRIORITY, notification->priority());
  // Should read the content again because the state has changed.
  EXPECT_TRUE(notification->rich_notification_data()
                  .should_make_spoken_feedback_for_popup_updates);

  // Session length limit is updated to longer: 15 min.
  UpdateSessionLengthLimitInMin(15);
  notification = GetNotification();
  EXPECT_TRUE(notification);
  EXPECT_EQ(message_center::SYSTEM_PRIORITY, notification->priority());
  // Should read again because an increase of the remaining time is noteworthy.
  EXPECT_TRUE(notification->rich_notification_data()
                  .should_make_spoken_feedback_for_popup_updates);

  // Clears the limit: the notification should be gone.
  ClearSessionLengthLimit();
  EXPECT_FALSE(GetNotification());
}

TEST_F(TraySessionLengthLimitTest, RemoveNotification) {
  // Limit is 15 min.
  UpdateSessionLengthLimitInMin(15);
  EXPECT_TRUE(GetNotification());

  // Removes the notification.
  RemoveNotification();
  EXPECT_FALSE(GetNotification());

  // Limit is 10 min. The notification should not re-appear.
  UpdateSessionLengthLimitInMin(10);
  EXPECT_FALSE(GetNotification());

  // Limit is 3 min. The notification should re-appear and should be re-read
  // because of state change.
  UpdateSessionLengthLimitInMin(3);
  message_center::Notification* notification = GetNotification();
  EXPECT_TRUE(notification);
  EXPECT_TRUE(notification->rich_notification_data()
                  .should_make_spoken_feedback_for_popup_updates);

  RemoveNotification();

  // Session length limit is updated to longer state. Notification should
  // re-appear and be re-read.
  UpdateSessionLengthLimitInMin(15);
  notification = GetNotification();
  EXPECT_TRUE(notification);
  EXPECT_TRUE(notification->rich_notification_data()
                  .should_make_spoken_feedback_for_popup_updates);
}

class TraySessionLengthLimitLoginTest : public TraySessionLengthLimitTest {
 public:
  TraySessionLengthLimitLoginTest() { set_start_session(false); }

 private:
  DISALLOW_COPY_AND_ASSIGN(TraySessionLengthLimitLoginTest);
};

TEST_F(TraySessionLengthLimitLoginTest, NotificationShownAfterLogin) {
  UpdateSessionLengthLimitInMin(15);

  // No notifications before login.
  EXPECT_FALSE(GetNotification());

  // Notification is shown after login.
  SetSessionStarted(true);
  EXPECT_TRUE(GetNotification());
}

}  // namespace ash
