// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/supervised/tray_supervised_user.h"

#include "ash/common/login_status.h"
#include "ash/common/test/ash_test.h"
#include "ash/common/test/test_system_tray_delegate.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_list.h"
#include "ui/message_center/notification_types.h"

using message_center::NotificationList;

namespace ash {

class TraySupervisedUserTest : public AshTest {
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

class TraySupervisedUserInitialTest : public TraySupervisedUserTest {
 public:
  // Set the initial login status to supervised-user before AshTest::SetUp()
  // constructs the system tray.
  TraySupervisedUserInitialTest()
      : scoped_initial_login_status_(LoginStatus::SUPERVISED) {}
  ~TraySupervisedUserInitialTest() override {}

 private:
  test::ScopedInitialLoginStatus scoped_initial_login_status_;

  DISALLOW_COPY_AND_ASSIGN(TraySupervisedUserInitialTest);
};

TEST_F(TraySupervisedUserTest, SupervisedUserHasNotification) {
  test::TestSystemTrayDelegate* delegate = GetSystemTrayDelegate();
  delegate->SetLoginStatus(LoginStatus::SUPERVISED);

  message_center::Notification* notification = GetPopup();
  ASSERT_NE(static_cast<message_center::Notification*>(NULL), notification);
  EXPECT_EQ(static_cast<int>(message_center::SYSTEM_PRIORITY),
            notification->rich_notification_data().priority);
}

TEST_F(TraySupervisedUserInitialTest, SupervisedUserNoCrash) {
  // Initial login status is already SUPERVISED, which should create
  // the notification and should not cause crashes.
  message_center::Notification* notification = GetPopup();
  ASSERT_NE(static_cast<message_center::Notification*>(NULL), notification);
  EXPECT_EQ(static_cast<int>(message_center::SYSTEM_PRIORITY),
            notification->rich_notification_data().priority);
}

}  // namespace ash
