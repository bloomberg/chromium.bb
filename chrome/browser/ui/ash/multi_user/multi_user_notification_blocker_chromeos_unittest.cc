// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/system_notifier.h"
#include "ash/common/wm_shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_shell_delegate.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/browser/chromeos/login/users/wallpaper/wallpaper_manager.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_notification_blocker_chromeos.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_chromeos.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/signin/core/account_id/account_id.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"

using base::UTF8ToUTF16;

class MultiUserNotificationBlockerChromeOSTest
    : public ash::test::AshTestBase,
      public message_center::NotificationBlocker::Observer {
 public:
  MultiUserNotificationBlockerChromeOSTest()
      : state_changed_count_(0),
        testing_profile_manager_(TestingBrowserProcess::GetGlobal()),
        window_id_(0),
        fake_user_manager_(new chromeos::FakeChromeUserManager),
        user_manager_enabler_(fake_user_manager_) {}
  ~MultiUserNotificationBlockerChromeOSTest() override {}

  // ash::test::AshTestBase overrides:
  void SetUp() override {
    ash::test::AshTestBase::SetUp();
    ASSERT_TRUE(testing_profile_manager_.SetUp());

    // MultiUserWindowManager is initialized after the log in.
    testing_profile_manager_.CreateTestingProfile(GetDefaultUserId());
    fake_user_manager_->AddUser(AccountId::FromUserEmail(GetDefaultUserId()));

    ash::test::TestShellDelegate* shell_delegate =
        static_cast<ash::test::TestShellDelegate*>(
            ash::WmShell::Get()->delegate());
    shell_delegate->set_multi_profiles_enabled(true);
    chrome::MultiUserWindowManager::CreateInstance();

    const AccountId test2_account_id =
        AccountId::FromUserEmail("test2@example.com");
    fake_user_manager_->AddUser(test2_account_id);

    chromeos::WallpaperManager::Initialize();

    // Disable any animations for the test.
    GetMultiUserWindowManager()->SetAnimationSpeedForTest(
        chrome::MultiUserWindowManagerChromeOS::ANIMATION_SPEED_DISABLED);
    GetMultiUserWindowManager()->notification_blocker_->AddObserver(this);
  }

  void TearDown() override {
    GetMultiUserWindowManager()->notification_blocker_->RemoveObserver(this);
    if (chrome::MultiUserWindowManager::GetInstance())
      chrome::MultiUserWindowManager::DeleteInstance();
    ash::test::AshTestBase::TearDown();
    chromeos::WallpaperManager::Shutdown();
  }

  // message_center::NotificationBlocker::Observer ovverrides:
  void OnBlockingStateChanged(
      message_center::NotificationBlocker* blocker) override {
    state_changed_count_++;
  }

 protected:
  chrome::MultiUserWindowManagerChromeOS* GetMultiUserWindowManager() {
    return static_cast<chrome::MultiUserWindowManagerChromeOS*>(
        chrome::MultiUserWindowManager::GetInstance());
  }

  const std::string GetDefaultUserId() { return "test1@example.com"; }

  const message_center::NotificationBlocker* blocker() {
    return GetMultiUserWindowManager()->notification_blocker_.get();
  }

  void CreateProfile(const std::string& name) {
    testing_profile_manager_.CreateTestingProfile(name);
  }

  void SwitchActiveUser(const std::string& name) {
    const AccountId account_id(AccountId::FromUserEmail(name));
    if (chrome::MultiUserWindowManager::GetMultiProfileMode() ==
        chrome::MultiUserWindowManager::MULTI_PROFILE_MODE_SEPARATED) {
      static_cast<chrome::MultiUserWindowManagerChromeOS*>(
          chrome::MultiUserWindowManager::GetInstance())
          ->ActiveUserChanged(fake_user_manager_->FindUser(account_id));
    }
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

    message_center::Notification notification(
        message_center::NOTIFICATION_TYPE_SIMPLE, "popup-id",
        UTF8ToUTF16("popup-title"), UTF8ToUTF16("popup-message"), gfx::Image(),
        UTF8ToUTF16("popup-source"), GURL(), id_with_profile,
        message_center::RichNotificationData(), nullptr);

    return blocker()->ShouldShowNotificationAsPopup(notification);
  }

  bool ShouldShowNotification(
      const message_center::NotifierId& notifier_id,
      const std::string profile_id) {
    message_center::NotifierId id_with_profile = notifier_id;
    id_with_profile.profile_id = profile_id;

    message_center::Notification notification(
        message_center::NOTIFICATION_TYPE_SIMPLE, "notification-id",
        UTF8ToUTF16("notification-title"), UTF8ToUTF16("notification-message"),
        gfx::Image(), UTF8ToUTF16("notification-source"), GURL(),
        id_with_profile, message_center::RichNotificationData(), nullptr);

    return blocker()->ShouldShowNotification(notification);
  }

  aura::Window* CreateWindowForProfile(const std::string& name) {
    aura::Window* window = CreateTestWindowInShellWithId(window_id_++);
    chrome::MultiUserWindowManager::GetInstance()->SetWindowOwner(
        window, AccountId::FromUserEmail(name));
    return window;
  }

 private:
  int state_changed_count_;
  TestingProfileManager testing_profile_manager_;
  int window_id_;

  // Owned by ScopedUserManagerEnabler.
  chromeos::FakeChromeUserManager* fake_user_manager_ = nullptr;
  chromeos::ScopedUserManagerEnabler user_manager_enabler_;

  DISALLOW_COPY_AND_ASSIGN(MultiUserNotificationBlockerChromeOSTest);
};

TEST_F(MultiUserNotificationBlockerChromeOSTest, Basic) {
  ASSERT_EQ(chrome::MultiUserWindowManager::MULTI_PROFILE_MODE_SEPARATED,
            chrome::MultiUserWindowManager::GetMultiProfileMode());

  message_center::NotifierId notifier_id(
      message_center::NotifierId::APPLICATION, "test-app");
  // Only allowed the system notifier.
  message_center::NotifierId ash_system_notifier(
      message_center::NotifierId::SYSTEM_COMPONENT,
      ash::system_notifier::kNotifierDisplay);
  // Other system notifiers should be treated as same as a normal notifier.
  message_center::NotifierId random_system_notifier(
      message_center::NotifierId::SYSTEM_COMPONENT, "random_system_component");

  EXPECT_FALSE(ShouldShowNotificationAsPopup(notifier_id, ""));
  EXPECT_TRUE(ShouldShowNotificationAsPopup(ash_system_notifier, ""));
  EXPECT_FALSE(ShouldShowNotificationAsPopup(random_system_notifier, ""));
  EXPECT_TRUE(ShouldShowNotificationAsPopup(notifier_id, GetDefaultUserId()));
  EXPECT_FALSE(ShouldShowNotification(notifier_id, ""));
  EXPECT_TRUE(ShouldShowNotification(ash_system_notifier, ""));
  EXPECT_FALSE(ShouldShowNotification(random_system_notifier, ""));
  EXPECT_TRUE(ShouldShowNotification(notifier_id, GetDefaultUserId()));
  EXPECT_TRUE(ShouldShowNotification(random_system_notifier,
                                     GetDefaultUserId()));

  CreateProfile("test2@example.com");
  EXPECT_EQ(0, GetStateChangedCountAndReset());
  EXPECT_FALSE(ShouldShowNotificationAsPopup(notifier_id, ""));
  EXPECT_TRUE(ShouldShowNotificationAsPopup(ash_system_notifier, ""));
  EXPECT_FALSE(ShouldShowNotificationAsPopup(random_system_notifier, ""));
  EXPECT_TRUE(ShouldShowNotificationAsPopup(notifier_id, GetDefaultUserId()));
  EXPECT_FALSE(ShouldShowNotificationAsPopup(notifier_id, "test2@example.com"));
  EXPECT_TRUE(ShouldShowNotificationAsPopup(random_system_notifier,
                                            GetDefaultUserId()));
  EXPECT_FALSE(ShouldShowNotificationAsPopup(random_system_notifier,
                                             "test2@example.com"));
  EXPECT_FALSE(ShouldShowNotification(notifier_id, ""));
  EXPECT_TRUE(ShouldShowNotification(ash_system_notifier, ""));
  EXPECT_FALSE(ShouldShowNotification(random_system_notifier, ""));
  EXPECT_TRUE(ShouldShowNotification(notifier_id, GetDefaultUserId()));
  EXPECT_FALSE(ShouldShowNotification(notifier_id, "test2@example.com"));
  EXPECT_TRUE(ShouldShowNotification(random_system_notifier,
                                     GetDefaultUserId()));
  EXPECT_FALSE(ShouldShowNotification(random_system_notifier,
                                      "test2@example.com"));

  SwitchActiveUser("test2@example.com");
  EXPECT_FALSE(ShouldShowNotificationAsPopup(notifier_id, ""));
  EXPECT_TRUE(ShouldShowNotificationAsPopup(ash_system_notifier, ""));
  EXPECT_FALSE(ShouldShowNotificationAsPopup(random_system_notifier, ""));
  EXPECT_FALSE(ShouldShowNotificationAsPopup(notifier_id, GetDefaultUserId()));
  EXPECT_TRUE(ShouldShowNotificationAsPopup(notifier_id, "test2@example.com"));
  EXPECT_FALSE(ShouldShowNotificationAsPopup(random_system_notifier,
                                             GetDefaultUserId()));
  EXPECT_TRUE(ShouldShowNotificationAsPopup(random_system_notifier,
                                            "test2@example.com"));
  EXPECT_FALSE(ShouldShowNotification(notifier_id, ""));
  EXPECT_TRUE(ShouldShowNotification(ash_system_notifier, ""));
  EXPECT_FALSE(ShouldShowNotification(random_system_notifier, ""));
  EXPECT_FALSE(ShouldShowNotification(notifier_id, GetDefaultUserId()));
  EXPECT_TRUE(ShouldShowNotification(notifier_id, "test2@example.com"));
  EXPECT_FALSE(ShouldShowNotification(random_system_notifier,
                                      GetDefaultUserId()));
  EXPECT_TRUE(ShouldShowNotification(random_system_notifier,
                                     "test2@example.com"));

  SwitchActiveUser(GetDefaultUserId());
  EXPECT_FALSE(ShouldShowNotificationAsPopup(notifier_id, ""));
  EXPECT_TRUE(ShouldShowNotificationAsPopup(ash_system_notifier, ""));
  EXPECT_FALSE(ShouldShowNotificationAsPopup(random_system_notifier, ""));
  EXPECT_TRUE(ShouldShowNotificationAsPopup(notifier_id, GetDefaultUserId()));
  EXPECT_FALSE(ShouldShowNotificationAsPopup(notifier_id, "test2@example.com"));
  EXPECT_TRUE(ShouldShowNotificationAsPopup(random_system_notifier,
                                            GetDefaultUserId()));
  EXPECT_FALSE(ShouldShowNotificationAsPopup(random_system_notifier,
                                             "test2@example.com"));
  EXPECT_FALSE(ShouldShowNotification(notifier_id, ""));
  EXPECT_TRUE(ShouldShowNotification(ash_system_notifier, ""));
  EXPECT_FALSE(ShouldShowNotification(random_system_notifier, ""));
  EXPECT_TRUE(ShouldShowNotification(notifier_id, GetDefaultUserId()));
  EXPECT_FALSE(ShouldShowNotification(notifier_id, "test2@example.com"));
  EXPECT_TRUE(ShouldShowNotification(random_system_notifier,
                                     GetDefaultUserId()));
  EXPECT_FALSE(ShouldShowNotification(random_system_notifier,
                                      "test2@example.com"));
}
