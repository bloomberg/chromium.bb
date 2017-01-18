// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/session/tray_session_length_limit.h"

#include "ash/common/system/tray/system_tray.h"
#include "ash/common/test/test_system_tray_delegate.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_types.h"

namespace ash {
namespace test {

class TraySessionLengthLimitTest : public AshTestBase {
 public:
  TraySessionLengthLimitTest() {}
  ~TraySessionLengthLimitTest() override {}

  void SetUp() override {
    AshTestBase::SetUp();
    SystemTray* system_tray = GetPrimarySystemTray();
    tray_session_length_limit_ = new TraySessionLengthLimit(system_tray);
    system_tray->AddTrayItem(base::WrapUnique(tray_session_length_limit_));
  }

  void TearDown() override {
    ClearSessionLengthLimit();
    AshTestBase::TearDown();
  }

 protected:
  void UpdateSessionLengthLimitInMin(int mins) {
    GetSystemTrayDelegate()->SetSessionLengthLimitForTest(
        base::TimeDelta::FromMinutes(mins));
    tray_session_length_limit_->OnSessionLengthLimitChanged();
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
    GetSystemTrayDelegate()->ClearSessionLengthLimit();
    tray_session_length_limit_->OnSessionLengthLimitChanged();
  }

  void RemoveNotification() {
    message_center::MessageCenter::Get()->RemoveNotification(
        TraySessionLengthLimit::kNotificationId, false /* by_user */);
  }

  TraySessionLengthLimit* tray_session_length_limit() {
    return tray_session_length_limit_;
  }

 private:
  // Weak reference, owned by the SystemTray.
  TraySessionLengthLimit* tray_session_length_limit_;

  DISALLOW_COPY_AND_ASSIGN(TraySessionLengthLimitTest);
};

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

}  // namespace test
}  // namespace ash
