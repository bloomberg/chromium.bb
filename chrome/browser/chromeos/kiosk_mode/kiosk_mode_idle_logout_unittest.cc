// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/kiosk_mode/kiosk_mode_idle_logout.h"

#include "ash/test/ash_test_base.h"
#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/settings/device_settings_test_helper.h"
#include "chrome/common/chrome_notification_types.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace chromeos {

class KioskModeIdleLogoutTest : public ash::test::AshTestBase {
 public:
  KioskModeIdleLogoutTest()
      : ui_thread_(BrowserThread::UI, message_loop()),
        idle_logout_(NULL) {
  }

  virtual void SetUp() OVERRIDE {
    DBusThreadManager::Initialize();
    AshTestBase::SetUp();
    idle_logout_ = new KioskModeIdleLogout();
  }

  virtual void TearDown() OVERRIDE {
    delete idle_logout_;
    AshTestBase::TearDown();
    DBusThreadManager::Shutdown();
  }

  bool LoginUserObserverRegistered() {
    return idle_logout_->registrar_.IsRegistered(
        idle_logout_,
        chrome::NOTIFICATION_LOGIN_USER_CHANGED,
        content::NotificationService::AllSources());
  }

  bool PowerManagerObserverRegistered() {
    chromeos::PowerManagerClient* power_manager =
        chromeos::DBusThreadManager::Get()->GetPowerManagerClient();
    return power_manager->HasObserver(idle_logout_);
  }

  content::TestBrowserThread ui_thread_;

  ScopedDeviceSettingsTestHelper device_settings_test_helper_;

  KioskModeIdleLogout* idle_logout_;
  content::NotificationRegistrar registrar_;
};

// http://crbug.com/177918
TEST_F(KioskModeIdleLogoutTest, DISABLED_CheckObserversBeforeUserLogin) {
  EXPECT_TRUE(LoginUserObserverRegistered());
  EXPECT_FALSE(PowerManagerObserverRegistered());
}

// http://crbug.com/177918
TEST_F(KioskModeIdleLogoutTest, DISABLED_CheckObserversAfterUserLogin) {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_LOGIN_USER_CHANGED,
      content::Source<UserManager>(UserManager::Get()),
      // Ideally this should be the user logged in, but since we won't really be
      // checking for the current logged in user in our observer anyway, giving
      // NoDetails here is fine.
      content::NotificationService::NoDetails());

  RunAllPendingInMessageLoop();
  EXPECT_FALSE(LoginUserObserverRegistered());
  EXPECT_TRUE(PowerManagerObserverRegistered());
}

}  // namespace chromeos
