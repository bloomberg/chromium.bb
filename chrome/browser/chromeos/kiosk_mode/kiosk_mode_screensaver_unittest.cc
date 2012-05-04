// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/kiosk_mode/kiosk_mode_screensaver.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/chromeos/login/mock_user_manager.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chromeos/dbus/mock_dbus_thread_manager.h"
#include "chromeos/dbus/mock_power_manager_client.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;
using ::testing::SaveArg;

namespace chromeos {

ACTION_P(HasObserver, expected_pointer) {
  return arg0 == *expected_pointer;
}

ACTION_P(RemoveObserver, expected_pointer) {
  if (arg0 == *expected_pointer)
    *expected_pointer = NULL;
}

class KioskModeScreensaverTest : public testing::Test {
 public:
  KioskModeScreensaverTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        screensaver_(NULL),
        observer_(NULL) {
  }

  virtual void SetUp() OVERRIDE {
    EXPECT_CALL(*mock_user_manager_.user_manager(), IsUserLoggedIn())
        .Times(AnyNumber())
        .WillRepeatedly(Return(false));

    MockDBusThreadManager* mock_dbus_thread_manager =
        new MockDBusThreadManager;
    DBusThreadManager::InitializeForTesting(mock_dbus_thread_manager);

    MockPowerManagerClient* power_manager =
        mock_dbus_thread_manager->mock_power_manager_client();
    EXPECT_CALL(*power_manager, HasObserver(_))
        .Times(AnyNumber())
        .WillRepeatedly(HasObserver(&observer_));
    EXPECT_CALL(*power_manager, AddObserver(_))
        .WillOnce(SaveArg<0>(&observer_));
    EXPECT_CALL(*power_manager, RemoveObserver(_))
        .WillOnce(RemoveObserver(&observer_));
    EXPECT_CALL(*power_manager, RequestIdleNotification(_))
        .Times(AnyNumber());

    screensaver_ = new KioskModeScreensaver();
    screensaver_->SetupScreensaver(NULL, NULL, FilePath());
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
    PowerManagerClient* power_manager =
        DBusThreadManager::Get()->GetPowerManagerClient();
    return power_manager->HasObserver(screensaver_);
  }

  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;

  ScopedMockUserManagerEnabler mock_user_manager_;

  KioskModeScreensaver* screensaver_;
  content::NotificationRegistrar registrar_;

  PowerManagerClient::Observer* observer_;
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
