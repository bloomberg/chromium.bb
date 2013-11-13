// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"
#include "ash/system/system_notifier.h"
#include "base/command_line.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/user_adding_screen.h"
#include "chrome/browser/notifications/login_state_notification_blocker_chromeos.h"
#include "chrome/common/chrome_switches.h"
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

  // InProcessBrowserTest overrides:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    LoginManagerTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(::switches::kMultiProfiles);
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    chromeos::LoginState::Get()->set_always_logged_in(false);
  }

 protected:
  scoped_ptr<message_center::NotificationBlocker> CreateBlocker() {
    LoginStateNotificationBlockerChromeOS* blocker =
        new LoginStateNotificationBlockerChromeOS(
            message_center::MessageCenter::Get());
    blocker->AddObserver(this);
    return scoped_ptr<message_center::NotificationBlocker>(blocker);
  }

  // message_center::NotificationBlocker::Observer ovverrides:
  virtual void OnBlockingStateChanged() OVERRIDE {
    state_changed_count_++;
  }

  int GetStateChangedCountAndReset() {
    int result = state_changed_count_;
    state_changed_count_ = 0;
    return result;
  }

 private:
  int state_changed_count_;

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
  scoped_ptr<message_center::NotificationBlocker> blocker(CreateBlocker());
  message_center::NotifierId notifier_id;

  // Logged in as a normal user.
  EXPECT_CALL(login_utils(), DoBrowserLaunch(_, _)).Times(1);
  LoginUser(kTestUsers[0]);
  EXPECT_EQ(1, GetStateChangedCountAndReset());
  EXPECT_TRUE(blocker->ShouldShowNotificationAsPopup(notifier_id));

  // Multi-login user switch.
  chromeos::UserAddingScreen::Get()->Start();
  content::RunAllPendingInMessageLoop();
  EXPECT_EQ(1, GetStateChangedCountAndReset());
  EXPECT_FALSE(blocker->ShouldShowNotificationAsPopup(notifier_id));

  // Multi-login user switch off.
  chromeos::UserAddingScreen::Get()->Cancel();
  content::RunAllPendingInMessageLoop();
  EXPECT_EQ(1, GetStateChangedCountAndReset());
  EXPECT_TRUE(blocker->ShouldShowNotificationAsPopup(notifier_id));
}

IN_PROC_BROWSER_TEST_F(LoginStateNotificationBlockerChromeOSBrowserTest,
                       PRE_AlwaysAllowedNotifier) {
  RegisterUser(kTestUsers[0]);
  RegisterUser(kTestUsers[1]);
  chromeos::StartupUtils::MarkOobeCompleted();
}

IN_PROC_BROWSER_TEST_F(LoginStateNotificationBlockerChromeOSBrowserTest,
                       AlwaysAllowedNotifier) {
  scoped_ptr<message_center::NotificationBlocker> blocker(CreateBlocker());

  // NOTIFIER_DISPLAY is allowed to shown in the login screen.
  message_center::NotifierId notifier_id(
      ash::system_notifier::NOTIFIER_DISPLAY);

  // Logged in as a normal user.
  EXPECT_CALL(login_utils(), DoBrowserLaunch(_, _)).Times(1);
  LoginUser(kTestUsers[0]);
  EXPECT_EQ(1, GetStateChangedCountAndReset());
  EXPECT_TRUE(blocker->ShouldShowNotificationAsPopup(notifier_id));

  // Multi-login user switch.
  chromeos::UserAddingScreen::Get()->Start();
  content::RunAllPendingInMessageLoop();
  EXPECT_EQ(1, GetStateChangedCountAndReset());
  EXPECT_TRUE(blocker->ShouldShowNotificationAsPopup(notifier_id));

  // Multi-login user switch off.
  chromeos::UserAddingScreen::Get()->Cancel();
  content::RunAllPendingInMessageLoop();
  EXPECT_EQ(1, GetStateChangedCountAndReset());
  EXPECT_TRUE(blocker->ShouldShowNotificationAsPopup(notifier_id));
}
