// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/kiosk_mode/kiosk_mode_screensaver.h"

#include "ash/test/ash_test_base.h"
#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/chromeos/login/mock_user_manager.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;
using ::testing::SaveArg;

namespace chromeos {

class KioskModeScreensaverTest : public ash::test::AshTestBase {
 public:
  KioskModeScreensaverTest()
      : ui_thread_(BrowserThread::UI, message_loop()),
        file_thread_(BrowserThread::FILE, message_loop()),
        screensaver_(NULL) {
  }

  virtual void SetUp() OVERRIDE {
    // We need this so that the GetDefaultProfile call from within
    // SetupScreensaver doesn't crash.
    profile_manager_.reset(new TestingProfileManager(
        static_cast<TestingBrowserProcess*>(g_browser_process)));
    ASSERT_TRUE(profile_manager_->SetUp());

    AshTestBase::SetUp();
    EXPECT_CALL(*mock_user_manager_.user_manager(), IsUserLoggedIn())
        .Times(AnyNumber())
        .WillRepeatedly(Return(false));

    screensaver_ = new KioskModeScreensaver();
    screensaver_->SetupScreensaver(NULL, FilePath());
  }

  virtual void TearDown() OVERRIDE {
    delete screensaver_;
    AshTestBase::TearDown();
  }

  bool LoginUserObserverRegistered() {
    return screensaver_->registrar_.IsRegistered(
        screensaver_,
        chrome::NOTIFICATION_SESSION_STARTED,
        content::NotificationService::AllSources());
  }

  ScopedMockUserManagerEnabler mock_user_manager_;

  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

  KioskModeScreensaver* screensaver_;
  content::NotificationRegistrar registrar_;

  scoped_ptr<TestingProfileManager> profile_manager_;
};

TEST_F(KioskModeScreensaverTest, CheckObservers) {
  EXPECT_TRUE(LoginUserObserverRegistered());
}

TEST_F(KioskModeScreensaverTest, CheckObserversAfterUserLogin) {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_SESSION_STARTED,
      content::Source<UserManager>(UserManager::Get()),
      content::NotificationService::NoDetails());

  RunAllPendingInMessageLoop();
  EXPECT_FALSE(LoginUserObserverRegistered());
}

}  // namespace chromeos
