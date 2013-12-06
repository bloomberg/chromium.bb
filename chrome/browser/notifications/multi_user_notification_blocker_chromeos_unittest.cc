// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_shell_delegate.h"
#include "chrome/browser/chromeos/login/fake_user_manager.h"
#include "chrome/browser/notifications/multi_user_notification_blocker_chromeos.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"

namespace {

// This value should be same as the one in fake_user_manager.cc
static const char kUserIdHashSuffix[] = "-hash";

}

class MultiUserNotificationBlockerChromeOSTest
    : public ash::test::AshTestBase,
      public message_center::NotificationBlocker::Observer {
 public:
  MultiUserNotificationBlockerChromeOSTest()
      : state_changed_count_(0),
        is_logged_in_(false),
        testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  virtual ~MultiUserNotificationBlockerChromeOSTest() {}

  // ash::test::AshTestBase overrides:
  virtual void SetUp() OVERRIDE {
    ash::test::AshTestBase::SetUp();
    ASSERT_TRUE(testing_profile_manager_.SetUp());

    // Initialize the UserManager singleton to a fresh FakeUserManager instance.
    user_manager_enabler_.reset(
        new chromeos::ScopedUserManagerEnabler(new chromeos::FakeUserManager));
    blocker_.reset(new MultiUserNotificationBlockerChromeOS(
        message_center::MessageCenter::Get()));
    blocker_->AddObserver(this);
  }

  virtual void TearDown() OVERRIDE {
    blocker_->RemoveObserver(this);
    blocker_.reset();
    if (chrome::MultiUserWindowManager::GetInstance())
      chrome::MultiUserWindowManager::DeleteInstance();
    ash::test::AshTestBase::TearDown();
  }

  // message_center::NotificationBlocker::Observer ovverrides:
  virtual void OnBlockingStateChanged(
      message_center::NotificationBlocker* blocker) OVERRIDE {
    state_changed_count_++;
  }

 protected:
  void SetupMultiUserMode(bool enabled) {
    ash::test::TestShellDelegate* shell_delegate =
        static_cast<ash::test::TestShellDelegate*>(
            ash::Shell::GetInstance()->delegate());
    shell_delegate->set_multi_profiles_enabled(enabled);
    chrome::MultiUserWindowManager::CreateInstance();
  }

  void CreateProfile(const std::string& name) {
    testing_profile_manager_.CreateTestingProfile(name);
    GetFakeUserManager()->AddUser(name);
    GetFakeUserManager()->UserLoggedIn(name, name + kUserIdHashSuffix, false);
    if (!is_logged_in_) {
      SwitchActiveUser(name);
      is_logged_in_ = true;
    }
  }

  void SwitchActiveUser(const std::string& name) {
    GetFakeUserManager()->SwitchActiveUser(name);
    blocker_->ActiveUserChanged(GetFakeUserManager()->GetActiveUser());
  }

  int GetStateChangedCountAndReset() {
    int result = state_changed_count_;
    state_changed_count_ = 0;
    return result;
  }

  bool ShouldShowNotificationAsPopup(
      const message_center::NotifierId& notifier_id,
      const std::string profile_id) {
    message_center::NotifierId id_with_profile = notifier_id;
    id_with_profile.profile_id = profile_id;
    return blocker_->ShouldShowNotificationAsPopup(id_with_profile);
  }

  bool ShouldShowNotification(
      const message_center::NotifierId& notifier_id,
      const std::string profile_id) {
    message_center::NotifierId id_with_profile = notifier_id;
    id_with_profile.profile_id = profile_id;
    return blocker_->ShouldShowNotification(id_with_profile);
  }

 private:
  chromeos::FakeUserManager* GetFakeUserManager() {
    return static_cast<chromeos::FakeUserManager*>(
        chromeos::UserManager::Get());
  }

  int state_changed_count_;
  bool is_logged_in_;
  TestingProfileManager testing_profile_manager_;
  scoped_ptr<chromeos::ScopedUserManagerEnabler> user_manager_enabler_;
  scoped_ptr<MultiUserNotificationBlockerChromeOS> blocker_;

  DISALLOW_COPY_AND_ASSIGN(MultiUserNotificationBlockerChromeOSTest);
};

TEST_F(MultiUserNotificationBlockerChromeOSTest, Basic) {
  SetupMultiUserMode(true);
  ASSERT_EQ(chrome::MultiUserWindowManager::MULTI_PROFILE_MODE_SEPARATED,
            chrome::MultiUserWindowManager::GetMultiProfileMode());

  message_center::NotifierId notifier_id(
      message_center::NotifierId::APPLICATION, "test-app");
  message_center::NotifierId system_notifier(0);

  // Nothing is created, active_user_id_ should be empty.
  EXPECT_TRUE(ShouldShowNotificationAsPopup(notifier_id, ""));
  EXPECT_TRUE(ShouldShowNotificationAsPopup(system_notifier, ""));
  EXPECT_TRUE(ShouldShowNotification(notifier_id, ""));
  EXPECT_TRUE(ShouldShowNotification(system_notifier, ""));

  CreateProfile("test@example.com");
  EXPECT_EQ(1, GetStateChangedCountAndReset());
  EXPECT_FALSE(ShouldShowNotificationAsPopup(notifier_id, ""));
  EXPECT_TRUE(ShouldShowNotificationAsPopup(system_notifier, ""));
  EXPECT_TRUE(ShouldShowNotificationAsPopup(notifier_id, "test@example.com"));
  EXPECT_FALSE(ShouldShowNotification(notifier_id, ""));
  EXPECT_TRUE(ShouldShowNotification(system_notifier, ""));
  EXPECT_TRUE(ShouldShowNotification(notifier_id, "test@example.com"));

  CreateProfile("test2@example.com");
  EXPECT_EQ(0, GetStateChangedCountAndReset());
  EXPECT_FALSE(ShouldShowNotificationAsPopup(notifier_id, ""));
  EXPECT_TRUE(ShouldShowNotificationAsPopup(system_notifier, ""));
  EXPECT_TRUE(ShouldShowNotificationAsPopup(notifier_id, "test@example.com"));
  EXPECT_FALSE(ShouldShowNotificationAsPopup(notifier_id, "test2@example.com"));
  EXPECT_FALSE(ShouldShowNotification(notifier_id, ""));
  EXPECT_TRUE(ShouldShowNotification(system_notifier, ""));
  EXPECT_TRUE(ShouldShowNotification(notifier_id, "test@example.com"));
  EXPECT_FALSE(ShouldShowNotification(notifier_id, "test2@example.com"));

  SwitchActiveUser("test2@example.com");
  EXPECT_FALSE(ShouldShowNotificationAsPopup(notifier_id, ""));
  EXPECT_TRUE(ShouldShowNotificationAsPopup(system_notifier, ""));
  EXPECT_FALSE(ShouldShowNotificationAsPopup(notifier_id, "test@example.com"));
  EXPECT_TRUE(ShouldShowNotificationAsPopup(notifier_id, "test2@example.com"));
  EXPECT_FALSE(ShouldShowNotification(notifier_id, ""));
  EXPECT_TRUE(ShouldShowNotification(system_notifier, ""));
  EXPECT_FALSE(ShouldShowNotification(notifier_id, "test@example.com"));
  EXPECT_TRUE(ShouldShowNotification(notifier_id, "test2@example.com"));

  SwitchActiveUser("test@example.com");
  EXPECT_FALSE(ShouldShowNotificationAsPopup(notifier_id, ""));
  EXPECT_TRUE(ShouldShowNotificationAsPopup(system_notifier, ""));
  EXPECT_TRUE(ShouldShowNotificationAsPopup(notifier_id, "test@example.com"));
  EXPECT_FALSE(ShouldShowNotificationAsPopup(notifier_id, "test2@example.com"));
  EXPECT_FALSE(ShouldShowNotification(notifier_id, ""));
  EXPECT_TRUE(ShouldShowNotification(system_notifier, ""));
  EXPECT_TRUE(ShouldShowNotification(notifier_id, "test@example.com"));
  EXPECT_FALSE(ShouldShowNotification(notifier_id, "test2@example.com"));
}

TEST_F(MultiUserNotificationBlockerChromeOSTest, SingleUser) {
  SetupMultiUserMode(false);
  ASSERT_EQ(chrome::MultiUserWindowManager::MULTI_PROFILE_MODE_OFF,
            chrome::MultiUserWindowManager::GetMultiProfileMode());

  message_center::NotifierId notifier_id(
      message_center::NotifierId::APPLICATION, "test-app");
  message_center::NotifierId system_notifier(0);

  // Nothing is created, active_user_id_ should be empty.
  EXPECT_TRUE(ShouldShowNotificationAsPopup(notifier_id, ""));
  EXPECT_TRUE(ShouldShowNotificationAsPopup(system_notifier, ""));
  EXPECT_TRUE(ShouldShowNotification(notifier_id, ""));
  EXPECT_TRUE(ShouldShowNotification(system_notifier, ""));

  CreateProfile("test@example.com");
  EXPECT_EQ(1, GetStateChangedCountAndReset());
  EXPECT_TRUE(ShouldShowNotificationAsPopup(notifier_id, ""));
  EXPECT_TRUE(ShouldShowNotificationAsPopup(system_notifier, ""));
  EXPECT_TRUE(ShouldShowNotificationAsPopup(notifier_id, "test@example.com"));
  EXPECT_TRUE(ShouldShowNotification(notifier_id, ""));
  EXPECT_TRUE(ShouldShowNotification(system_notifier, ""));
  EXPECT_TRUE(ShouldShowNotification(notifier_id, "test@example.com"));
}
