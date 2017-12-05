// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/system/system_notifier.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/ui/user_adding_screen.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/test_utils.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_observer.h"

using base::UTF8ToUTF16;
using namespace testing;

namespace {

class UserAddingFinishObserver : public chromeos::UserAddingScreen::Observer {
 public:
  UserAddingFinishObserver() {
    chromeos::UserAddingScreen::Get()->AddObserver(this);
  }

  ~UserAddingFinishObserver() override {
    chromeos::UserAddingScreen::Get()->RemoveObserver(this);
  }

  void WaitUntilUserAddingFinishedOrCancelled() {
    if (finished_)
      return;
    run_loop_.reset(new base::RunLoop());
    run_loop_->Run();
  }

  void OnUserAddingFinished() override {
    finished_ = true;
    if (run_loop_)
      run_loop_->Quit();
  }

  void OnUserAddingStarted() override { finished_ = false; }

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
  bool finished_ = false;  // True if OnUserAddingFinished() has been called
                           // before WaitUntilUserAddingFinishedOrCancelled().
  DISALLOW_COPY_AND_ASSIGN(UserAddingFinishObserver);
};

}  // anonymous namespace

class LoginStateNotificationBlockerChromeOSBrowserTest
    : public chromeos::LoginManagerTest,
      public message_center::MessageCenterObserver {
 public:
  LoginStateNotificationBlockerChromeOSBrowserTest()
      : chromeos::LoginManagerTest(false) {
    struct {
      const char* email;
      const char* gaia_id;
    } const kTestUsers[] = {{"test-user@gmail.com", "1110001111"},
                            {"test-user1@gmail.com", "1111111111"}};
    for (size_t i = 0; i < arraysize(kTestUsers); ++i) {
      test_users_.emplace_back(AccountId::FromUserEmailGaiaId(
          kTestUsers[i].email, kTestUsers[i].gaia_id));
    }
  }

  ~LoginStateNotificationBlockerChromeOSBrowserTest() override {}

  void SetUpOnMainThread() override {
    message_center::MessageCenter::Get()->AddObserver(this);
    chromeos::LoginManagerTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    message_center::MessageCenter::Get()->RemoveObserver(this);
    chromeos::LoginManagerTest::TearDownOnMainThread();
  }

 protected:
  // message_center::MessageCenterObserver:
  void OnBlockingStateChanged(
      message_center::NotificationBlocker* blocker) override {
    ++state_changed_count_;

    if (wait_loop_ && state_changed_count_ == expected_state_change_)
      wait_loop_->Quit();
  }

  int GetStateChangedCountAndReset() {
    int result = state_changed_count_;
    state_changed_count_ = 0;
    return result;
  }

  void WaitForStateChangeAndReset(int expected_state_change) {
    expected_state_change_ = expected_state_change;

    if (state_changed_count_ != expected_state_change_) {
      wait_loop_ = std::make_unique<base::RunLoop>();
      wait_loop_->Run();
      wait_loop_.reset();
    }

    EXPECT_EQ(expected_state_change_, state_changed_count_);

    expected_state_change_ = 0;
    state_changed_count_ = 0;
  }

  // Compares the number of notifications before and after adding a notification
  // to verify whether the new one is shown.
  bool ShouldShowNotificationAsPopup(
      const message_center::NotifierId& notifier_id) {
    size_t initial_count =
        message_center::MessageCenter::Get()->GetPopupNotifications().size();
    std::string id("browser-id");
    message_center::MessageCenter::Get()->AddNotification(
        std::make_unique<message_center::Notification>(
            message_center::NOTIFICATION_TYPE_SIMPLE, id,
            UTF8ToUTF16("browser-title"), UTF8ToUTF16("browser-message"),
            gfx::Image(), UTF8ToUTF16("browser-source"), GURL(), notifier_id,
            message_center::RichNotificationData(), nullptr));
    size_t new_count =
        message_center::MessageCenter::Get()->GetPopupNotifications().size();
    message_center::MessageCenter::Get()->RemoveNotification(id, false);
    return new_count == initial_count + 1;
  }

  std::vector<AccountId> test_users_;

 private:
  int state_changed_count_ = 0;

  std::unique_ptr<base::RunLoop> wait_loop_;
  int expected_state_change_ = 0;

  DISALLOW_COPY_AND_ASSIGN(LoginStateNotificationBlockerChromeOSBrowserTest);
};

IN_PROC_BROWSER_TEST_F(LoginStateNotificationBlockerChromeOSBrowserTest,
                       PRE_BaseTest) {
  RegisterUser(test_users_[0]);
  RegisterUser(test_users_[1]);
  chromeos::StartupUtils::MarkOobeCompleted();
}

IN_PROC_BROWSER_TEST_F(LoginStateNotificationBlockerChromeOSBrowserTest,
                       BaseTest) {
  message_center::NotifierId notifier_id(
      message_center::NotifierId::APPLICATION, "test-notifier");
  notifier_id.profile_id = test_users_[0].GetUserEmail();

  // Logged in as a normal user.
  LoginUser(test_users_[0]);

  // One state change from LoginStateNotificationBloker plus one state change
  // for the InactiveUserNotificationBlocker.
  WaitForStateChangeAndReset(2);
  EXPECT_TRUE(ShouldShowNotificationAsPopup(notifier_id));

  // Multi-login user switch.
  UserAddingFinishObserver observer;
  chromeos::UserAddingScreen::Get()->Start();
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetStateChangedCountAndReset());
  EXPECT_FALSE(ShouldShowNotificationAsPopup(notifier_id));

  // Multi-login user switch off.
  chromeos::UserAddingScreen::Get()->Cancel();
  observer.WaitUntilUserAddingFinishedOrCancelled();
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetStateChangedCountAndReset());
  EXPECT_TRUE(ShouldShowNotificationAsPopup(notifier_id));
}

IN_PROC_BROWSER_TEST_F(LoginStateNotificationBlockerChromeOSBrowserTest,
                       PRE_AlwaysAllowedNotifier) {
  RegisterUser(test_users_[0]);
  RegisterUser(test_users_[1]);
  chromeos::StartupUtils::MarkOobeCompleted();
}

IN_PROC_BROWSER_TEST_F(LoginStateNotificationBlockerChromeOSBrowserTest,
                       AlwaysAllowedNotifier) {
  // NOTIFIER_DISPLAY is allowed to shown in the login screen.
  message_center::NotifierId notifier_id(
      message_center::NotifierId::SYSTEM_COMPONENT,
      ash::system_notifier::kNotifierDisplay);
  notifier_id.profile_id = test_users_[0].GetUserEmail();

  // Logged in as a normal user.
  LoginUser(test_users_[0]);

  // One state change from LoginStateNotificationBloker plus one state change
  // for the InactiveUserNotificationBlocker.
  WaitForStateChangeAndReset(2);
  EXPECT_TRUE(ShouldShowNotificationAsPopup(notifier_id));

  // Multi-login user switch.
  UserAddingFinishObserver observer;
  chromeos::UserAddingScreen::Get()->Start();
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetStateChangedCountAndReset());
  EXPECT_TRUE(ShouldShowNotificationAsPopup(notifier_id));

  // Multi-login user switch off.
  chromeos::UserAddingScreen::Get()->Cancel();
  observer.WaitUntilUserAddingFinishedOrCancelled();
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetStateChangedCountAndReset());
  EXPECT_TRUE(ShouldShowNotificationAsPopup(notifier_id));
}
