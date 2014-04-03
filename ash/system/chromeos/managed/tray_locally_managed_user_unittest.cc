// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/managed/tray_locally_managed_user.h"

#include "ash/shell.h"
#include "ash/system/user/login_status.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_system_tray_delegate.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_list.h"
#include "ui/message_center/notification_types.h"

using message_center::NotificationList;

namespace ash {

class TrayLocallyManagedUserTest : public test::AshTestBase {
 public:
  TrayLocallyManagedUserTest() {}
  virtual ~TrayLocallyManagedUserTest() {}

 protected:
  message_center::Notification* GetPopup();

 private:
  DISALLOW_COPY_AND_ASSIGN(TrayLocallyManagedUserTest);
};

message_center::Notification* TrayLocallyManagedUserTest::GetPopup() {
  NotificationList::PopupNotifications popups =
      message_center::MessageCenter::Get()->GetPopupNotifications();
  for (NotificationList::PopupNotifications::const_iterator iter =
           popups.begin(); iter != popups.end(); ++iter) {
    if ((*iter)->id() == TrayLocallyManagedUser::kNotificationId)
      return *iter;
  }
  return NULL;
}

class TrayLocallyManagedUserInitialTest : public TrayLocallyManagedUserTest {
 public:
  TrayLocallyManagedUserInitialTest() {}
  virtual ~TrayLocallyManagedUserInitialTest() {}

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(TrayLocallyManagedUserInitialTest);
};

void TrayLocallyManagedUserInitialTest::SetUp() {
  test::TestSystemTrayDelegate::SetInitialLoginStatus(
      user::LOGGED_IN_LOCALLY_MANAGED);
  test::AshTestBase::SetUp();
}

void TrayLocallyManagedUserInitialTest::TearDown() {
  test::AshTestBase::TearDown();
  // SetInitialLoginStatus() is reset in AshTestHelper::TearDown().
}

TEST_F(TrayLocallyManagedUserTest, LocallyManagedUserHasNotification) {
  test::TestSystemTrayDelegate* delegate =
      static_cast<test::TestSystemTrayDelegate*>(
          ash::Shell::GetInstance()->system_tray_delegate());
  delegate->SetLoginStatus(user::LOGGED_IN_LOCALLY_MANAGED);

  message_center::Notification* notification = GetPopup();
  ASSERT_NE(static_cast<message_center::Notification*>(NULL), notification);
  EXPECT_EQ(static_cast<int>(message_center::SYSTEM_PRIORITY),
            notification->rich_notification_data().priority);
}

TEST_F(TrayLocallyManagedUserInitialTest, LocallyManagedUserNoCrash) {
  // Initial login status is already LOCALLY_MANAGED, which should create
  // the notification and should not cause crashes.
  message_center::Notification* notification = GetPopup();
  ASSERT_NE(static_cast<message_center::Notification*>(NULL), notification);
  EXPECT_EQ(static_cast<int>(message_center::SYSTEM_PRIORITY),
            notification->rich_notification_data().priority);
}

}  // namespace
