// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"
#include "ash/system/system_notifier.h"
#include "base/command_line.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/ui/user_adding_screen.h"
#include "chrome/browser/notifications/login_state_notification_blocker_chromeos.h"
#include "content/public/test/test_utils.h"
#include "ui/message_center/message_center.h"

using namespace testing;

namespace {

const char* kTestUsers[] = {"test-user@gmail.com",
                            "test-user1@gmail.com"};

}  // anonymous namespace

class LoginStateNotificationBlockerChromeOSBrowserTest
    : public chromeos::LoginManagerTest,
      public message_center::NotificationBlocker::Observer {
 public:
  LoginStateNotificationBlockerChromeOSBrowserTest()
      : chromeos::LoginManagerTest(false),
        state_changed_count_(0) {}
  virtual ~LoginStateNotificationBlockerChromeOSBrowserTest() {}

  virtual void SetUpOnMainThread() OVERRIDE {
    chromeos::LoginState::Get()->set_always_logged_in(false);
    chromeos::LoginManagerTest::SetUpOnMainThread();
  }

  virtual void TearDownOnMainThread() OVERRIDE {
    if (blocker_)
      blocker_->RemoveObserver(this);
    blocker_.reset();
    chromeos::LoginManagerTest::TearDownOnMainThread();
  }

 protected:
  void CreateBlocker() {
    blocker_.reset(new LoginStateNotificationBlockerChromeOS(
        message_center::MessageCenter::Get()));
    blocker_->AddObserver(this);
  }

  // message_center::NotificationBlocker::Observer ovverrides:
  virtual void OnBlockingStateChanged(
      message_center::NotificationBlocker* blocker) OVERRIDE {
    state_changed_count_++;
  }

  int GetStateChangedCountAndReset() {
    int result = state_changed_count_;
    state_changed_count_ = 0;
    return result;
  }

  bool ShouldShowNotificationAsPopup(
      const message_center::NotifierId& notifier_id) {
    return blocker_->ShouldShowNotificationAsPopup(notifier_id);
  }

 private:
  int state_changed_count_;
  scoped_ptr<message_center::NotificationBlocker> blocker_;

  DISALLOW_COPY_AND_ASSIGN(LoginStateNotificationBlockerChromeOSBrowserTest);
};

IN_PROC_BROWSER_TEST_F(LoginStateNotificationBlockerChromeOSBrowserTest,
                       PRE_BaseTest) {
  RegisterUser(kTestUsers[0]);
  RegisterUser(kTestUsers[1]);
  chromeos::StartupUtils::MarkOobeCompleted();
}

IN_PROC_BROWSER_TEST_F(LoginStateNotificationBlockerChromeOSBrowserTest,
                       BaseTest) {
  CreateBlocker();
  message_center::NotifierId notifier_id(
      message_center::NotifierId::APPLICATION, "test-notifier");

  // Logged in as a normal user.
  EXPECT_CALL(login_utils(), DoBrowserLaunch(_, _)).Times(1);
  LoginUser(kTestUsers[0]);
  EXPECT_EQ(1, GetStateChangedCountAndReset());
  EXPECT_TRUE(ShouldShowNotificationAsPopup(notifier_id));

  // Multi-login user switch.
  chromeos::UserAddingScreen::Get()->Start();
  content::RunAllPendingInMessageLoop();
  EXPECT_EQ(1, GetStateChangedCountAndReset());
  EXPECT_FALSE(ShouldShowNotificationAsPopup(notifier_id));

  // Multi-login user switch off.
  chromeos::UserAddingScreen::Get()->Cancel();
  content::RunAllPendingInMessageLoop();
  EXPECT_EQ(1, GetStateChangedCountAndReset());
  EXPECT_TRUE(ShouldShowNotificationAsPopup(notifier_id));
}

IN_PROC_BROWSER_TEST_F(LoginStateNotificationBlockerChromeOSBrowserTest,
                       PRE_AlwaysAllowedNotifier) {
  RegisterUser(kTestUsers[0]);
  RegisterUser(kTestUsers[1]);
  chromeos::StartupUtils::MarkOobeCompleted();
}

IN_PROC_BROWSER_TEST_F(LoginStateNotificationBlockerChromeOSBrowserTest,
                       AlwaysAllowedNotifier) {
  CreateBlocker();

  // NOTIFIER_DISPLAY is allowed to shown in the login screen.
  message_center::NotifierId notifier_id(
      message_center::NotifierId::SYSTEM_COMPONENT,
      ash::system_notifier::kNotifierDisplay);

  // Logged in as a normal user.
  EXPECT_CALL(login_utils(), DoBrowserLaunch(_, _)).Times(1);
  LoginUser(kTestUsers[0]);
  EXPECT_EQ(1, GetStateChangedCountAndReset());
  EXPECT_TRUE(ShouldShowNotificationAsPopup(notifier_id));

  // Multi-login user switch.
  chromeos::UserAddingScreen::Get()->Start();
  content::RunAllPendingInMessageLoop();
  EXPECT_EQ(1, GetStateChangedCountAndReset());
  EXPECT_TRUE(ShouldShowNotificationAsPopup(notifier_id));

  // Multi-login user switch off.
  chromeos::UserAddingScreen::Get()->Cancel();
  content::RunAllPendingInMessageLoop();
  EXPECT_EQ(1, GetStateChangedCountAndReset());
  EXPECT_TRUE(ShouldShowNotificationAsPopup(notifier_id));
}
