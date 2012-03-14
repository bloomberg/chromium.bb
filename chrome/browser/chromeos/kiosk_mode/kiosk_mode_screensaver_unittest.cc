// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/kiosk_mode/kiosk_mode_screensaver.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/chromeos/dbus/dbus_thread_manager.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace chromeos {

class KioskModeScreensaverTest : public testing::Test {
 public:
  KioskModeScreensaverTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        screensaver_(NULL) {
  }

  virtual void SetUp() OVERRIDE {
    DBusThreadManager::Initialize();
    screensaver_ = new KioskModeScreensaver();
  }

  virtual void TearDown() OVERRIDE {
    delete screensaver_;
    DBusThreadManager::Shutdown();
  }

  bool LoginUserObserverRegistered() {
    return screensaver_->registrar_.IsRegistered(
        screensaver_,
        chrome::NOTIFICATION_SESSION_STARTED,
        content::NotificationService::AllSources());
  }

  bool PowerManagerObserverRegistered() {
    chromeos::PowerManagerClient* power_manager =
                chromeos::DBusThreadManager::Get()->GetPowerManagerClient();
    return power_manager->HasObserver(screensaver_);
  }

  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;

  KioskModeScreensaver* screensaver_;
  content::NotificationRegistrar registrar_;
};

TEST_F(KioskModeScreensaverTest, CheckObservers) {
  EXPECT_TRUE(LoginUserObserverRegistered());
  EXPECT_TRUE(PowerManagerObserverRegistered());
}

TEST_F(KioskModeScreensaverTest, CheckObserversAfterUserLogin) {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_SESSION_STARTED,
      content::Source<UserManager>(UserManager::Get()),
      content::NotificationService::NoDetails());

  EXPECT_FALSE(PowerManagerObserverRegistered());
}

}  // namespace chromeos
