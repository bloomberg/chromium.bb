// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/supervised/tray_supervised_user.h"

#include "ash/login_status.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/system/tray/label_tray_view.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_test_api.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_session_controller_client.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_list.h"
#include "ui/message_center/notification_types.h"
#include "ui/views/view.h"

using message_center::NotificationList;

namespace ash {

// Tests handle creating their own sessions.
class TraySupervisedUserTest : public NoSessionAshTestBase {
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
  TestSessionControllerClient* client = GetSessionControllerClient();
  client->Reset();
  client->AddUserSession("child@test.com", user_manager::USER_TYPE_SUPERVISED);
  client->SetSessionState(session_manager::SessionState::ACTIVE);

  // No notification because custodian email not available yet.
  message_center::Notification* notification = GetPopup();
  EXPECT_FALSE(notification);

  // Update the user session with the custodian data (which happens after the
  // profile loads).
  mojom::UserSessionPtr user_session = session->GetUserSession(0)->Clone();
  user_session->custodian_email = "parent1@test.com";
  session->UpdateUserSession(std::move(user_session));

  // Notification is shown.
  notification = GetPopup();
  ASSERT_TRUE(notification);
  EXPECT_EQ(static_cast<int>(message_center::SYSTEM_PRIORITY),
            notification->rich_notification_data().priority);
  EXPECT_EQ(
      "Usage and history of this user can be reviewed by the manager "
      "(parent1@test.com) on chrome.com.",
      UTF16ToUTF8(notification->message()));

  // Update the user session with new custodian data.
  user_session = session->GetUserSession(0)->Clone();
  user_session->custodian_email = "parent2@test.com";
  session->UpdateUserSession(std::move(user_session));

  // Notification is shown with updated message.
  notification = GetPopup();
  ASSERT_TRUE(notification);
  EXPECT_EQ(
      "Usage and history of this user can be reviewed by the manager "
      "(parent2@test.com) on chrome.com.",
      UTF16ToUTF8(notification->message()));
}

// Verifies an item is created for a supervised user.
TEST_F(TraySupervisedUserTest, CreateDefaultView) {
  TraySupervisedUser* tray =
      SystemTrayTestApi(GetPrimarySystemTray()).tray_supervised_user();
  SessionController* session = Shell::Get()->session_controller();
  ASSERT_FALSE(session->IsActiveUserSessionStarted());

  // Before login there is no supervised user item.
  const LoginStatus unused = LoginStatus::NOT_LOGGED_IN;
  EXPECT_FALSE(tray->CreateDefaultView(unused));

  // Simulate a supervised user logging in.
  TestSessionControllerClient* client = GetSessionControllerClient();
  client->Reset();
  client->AddUserSession("child@test.com", user_manager::USER_TYPE_SUPERVISED);
  client->SetSessionState(session_manager::SessionState::ACTIVE);
  mojom::UserSessionPtr user_session = session->GetUserSession(0)->Clone();
  user_session->custodian_email = "parent@test.com";
  session->UpdateUserSession(std::move(user_session));

  // Now there is a supervised user item.
  std::unique_ptr<views::View> view =
      base::WrapUnique(tray->CreateDefaultView(unused));
  ASSERT_TRUE(view);
  EXPECT_EQ(
      "Usage and history of this user can be reviewed by the manager "
      "(parent@test.com) on chrome.com.",
      UTF16ToUTF8(static_cast<LabelTrayView*>(view.get())->message()));
}

}  // namespace ash
