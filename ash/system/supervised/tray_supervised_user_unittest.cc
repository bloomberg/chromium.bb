// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/supervised/tray_supervised_user.h"

#include "ash/login_status.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_session_controller_client.h"
#include "ash/test/test_system_tray_delegate.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_list.h"
#include "ui/message_center/notification_types.h"

using message_center::NotificationList;

namespace ash {

// Tests handle creating their own sessions.
class TraySupervisedUserTest : public test::NoSessionAshTestBase {
 public:
  TraySupervisedUserTest() {}
  ~TraySupervisedUserTest() override {}

 protected:
  message_center::Notification* GetPopup();

 private:
  DISALLOW_COPY_AND_ASSIGN(TraySupervisedUserTest);
};

message_center::Notification* TraySupervisedUserTest::GetPopup() {
  NotificationList::PopupNotifications popups =
      message_center::MessageCenter::Get()->GetPopupNotifications();
  for (NotificationList::PopupNotifications::const_iterator iter =
           popups.begin();
       iter != popups.end(); ++iter) {
    if ((*iter)->id() == TraySupervisedUser::kNotificationId)
      return *iter;
  }
  return NULL;
}

// Verifies that when a supervised user logs in that a warning notification is
// shown and ash does not crash.
TEST_F(TraySupervisedUserTest, SupervisedUserHasNotification) {
  SessionController* session = Shell::Get()->session_controller();
  ASSERT_EQ(LoginStatus::NOT_LOGGED_IN, session->login_status());
  ASSERT_FALSE(session->IsActiveUserSessionStarted());

  // Simulate a supervised user logging in.
  test::TestSessionControllerClient* client = GetSessionControllerClient();
  client->Reset();
  client->AddUserSession("user1@test.com", user_manager::USER_TYPE_SUPERVISED);
  client->SetSessionState(session_manager::SessionState::ACTIVE);

  message_center::Notification* notification = GetPopup();
  ASSERT_NE(static_cast<message_center::Notification*>(NULL), notification);
  EXPECT_EQ(static_cast<int>(message_center::SYSTEM_PRIORITY),
            notification->rich_notification_data().priority);
}

}  // namespace ash
