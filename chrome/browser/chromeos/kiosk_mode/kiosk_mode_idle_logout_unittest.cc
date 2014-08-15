// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/kiosk_mode/kiosk_mode_idle_logout.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/settings/device_settings_test_helper.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/wm/core/user_activity_detector.h"

namespace chromeos {

class KioskModeIdleLogoutTest : public ash::test::AshTestBase {
 public:
  KioskModeIdleLogoutTest()
      : idle_logout_(NULL) {
  }

  virtual void SetUp() OVERRIDE {
    AshTestBase::SetUp();
    idle_logout_ = new KioskModeIdleLogout();
  }

  virtual void TearDown() OVERRIDE {
    delete idle_logout_;
    AshTestBase::TearDown();
  }

  bool LoginUserObserverRegistered() {
    return idle_logout_->registrar_.IsRegistered(
        idle_logout_,
        chrome::NOTIFICATION_LOGIN_USER_CHANGED,
        content::NotificationService::AllSources());
  }

  bool UserActivityObserverRegistered() {
    return ash::Shell::GetInstance()->user_activity_detector()->HasObserver(
        idle_logout_);
  }

  ScopedDeviceSettingsTestHelper device_settings_test_helper_;

  KioskModeIdleLogout* idle_logout_;
  content::NotificationRegistrar registrar_;
};

// http://crbug.com/177918
TEST_F(KioskModeIdleLogoutTest, DISABLED_CheckObserversBeforeUserLogin) {
  EXPECT_TRUE(LoginUserObserverRegistered());
  EXPECT_FALSE(UserActivityObserverRegistered());
}

// http://crbug.com/177918
TEST_F(KioskModeIdleLogoutTest, DISABLED_CheckObserversAfterUserLogin) {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_LOGIN_USER_CHANGED,
      content::Source<user_manager::UserManager>(
          user_manager::UserManager::Get()),
      // Ideally this should be the user logged in, but since we won't really be
      // checking for the current logged in user in our observer anyway, giving
      // NoDetails here is fine.
      content::NotificationService::NoDetails());

  RunAllPendingInMessageLoop();
  EXPECT_FALSE(LoginUserObserverRegistered());
  EXPECT_TRUE(UserActivityObserverRegistered());
}

}  // namespace chromeos
