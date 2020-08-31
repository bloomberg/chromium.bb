// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdlib>
#include <cstring>
#include <memory>

#include "ash/public/cpp/ash_pref_names.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager_impl.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/test_wallpaper_controller.h"
#include "chrome/browser/ui/ash/wallpaper_controller_client.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/remove_user_delegate.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

class UnittestProfileManager : public ::ProfileManagerWithoutInit {
 public:
  explicit UnittestProfileManager(const base::FilePath& user_data_dir)
      : ::ProfileManagerWithoutInit(user_data_dir) {}

 protected:
  std::unique_ptr<Profile> CreateProfileHelper(
      const base::FilePath& path) override {
    if (!base::PathExists(path) && !base::CreateDirectory(path))
      return nullptr;
    return std::make_unique<TestingProfile>(path);
  }
};

class UnittestRemoveUserDelegate : public user_manager::RemoveUserDelegate {
 public:
  explicit UnittestRemoveUserDelegate(const AccountId& expected_account_id)
      : expected_account_id_(expected_account_id) {}

  bool HasBeforeUserRemoved() const { return has_before_user_removed_; }

  bool HasUserRemoved() const { return has_user_removed_; }

  void OnBeforeUserRemoved(const AccountId& account_id) override {
    has_before_user_removed_ = true;
    EXPECT_EQ(expected_account_id_, account_id);
  }

  void OnUserRemoved(const AccountId& account_id) override {
    has_user_removed_ = true;
    EXPECT_EQ(expected_account_id_, account_id);
  }

 private:
  const AccountId expected_account_id_;
  bool has_before_user_removed_;
  bool has_user_removed_;
};

class MockRemoveUserManager : public ChromeUserManagerImpl {
 public:
  MOCK_CONST_METHOD1(AsyncRemoveCryptohome, void(const AccountId&));
};

class UserManagerTest : public testing::Test {
 public:
  UserManagerTest() {}

 protected:
  void SetUp() override {
    base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();
    command_line.AppendSwitch(::switches::kTestType);
    command_line.AppendSwitch(
        chromeos::switches::kIgnoreUserProfileMappingForTests);

    settings_helper_.ReplaceDeviceSettingsProviderWithStub();

    // Populate the stub DeviceSettingsProvider with valid values.
    SetDeviceSettings(false, "", false);

    // Register an in-memory local settings instance.
    local_state_.reset(
        new ScopedTestingLocalState(TestingBrowserProcess::GetGlobal()));

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    TestingBrowserProcess::GetGlobal()->SetProfileManager(
        new UnittestProfileManager(temp_dir_.GetPath()));

    chromeos::DBusThreadManager::Initialize();

    ResetUserManager();

    wallpaper_controller_client_ =
        std::make_unique<WallpaperControllerClient>();
    wallpaper_controller_client_->InitForTesting(&test_wallpaper_controller_);
  }

  void TearDown() override {
    wallpaper_controller_client_.reset();

    // Shut down the DeviceSettingsService.
    DeviceSettingsService::Get()->UnsetSessionManager();
    TestingBrowserProcess::GetGlobal()->SetProfileManager(NULL);

    // Unregister the in-memory local settings instance.
    local_state_.reset();

    base::RunLoop().RunUntilIdle();
    chromeos::DBusThreadManager::Shutdown();
  }

  ChromeUserManagerImpl* GetChromeUserManager() const {
    return static_cast<ChromeUserManagerImpl*>(
        user_manager::UserManager::Get());
  }

  bool GetUserManagerEphemeralUsersEnabled() const {
    return GetChromeUserManager()->GetEphemeralUsersEnabled();
  }

  void SetUserManagerEphemeralUsersEnabled(bool ephemeral_users_enabled) {
    GetChromeUserManager()->SetEphemeralUsersEnabled(ephemeral_users_enabled);
  }

  AccountId GetUserManagerOwnerId() const {
    return GetChromeUserManager()->GetOwnerAccountId();
  }

  void SetUserManagerOwnerId(const AccountId& owner_account_id) {
    GetChromeUserManager()->SetOwnerId(owner_account_id);
  }

  void ResetUserManager() {
    // Reset the UserManager singleton.
    user_manager_enabler_.reset();
    // Initialize the UserManager singleton to a fresh ChromeUserManagerImpl
    // instance.
    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        ChromeUserManagerImpl::CreateChromeUserManager());

    // ChromeUserManagerImpl ctor posts a task to reload policies.
    base::RunLoop().RunUntilIdle();
  }

  std::unique_ptr<MockRemoveUserManager> CreateMockRemoveUserManager() const {
    return std::make_unique<MockRemoveUserManager>();
  }

  void SetDeviceSettings(bool ephemeral_users_enabled,
                         const std::string& owner,
                         bool supervised_users_enabled) {
    settings_helper_.SetBoolean(kAccountsPrefEphemeralUsersEnabled,
                                ephemeral_users_enabled);
    settings_helper_.SetString(kDeviceOwner, owner);
    settings_helper_.SetBoolean(kAccountsPrefSupervisedUsersEnabled,
                                supervised_users_enabled);
  }

  void RetrieveTrustedDevicePolicies() {
    GetChromeUserManager()->RetrieveTrustedDevicePolicies();
  }

  const AccountId owner_account_id_at_invalid_domain_ =
      AccountId::FromUserEmailGaiaId("owner@invalid.domain", "1234567890");
  const AccountId account_id0_at_invalid_domain_ =
      AccountId::FromUserEmailGaiaId("user0@invalid.domain", "0123456789");
  const AccountId account_id1_at_invalid_domain_ =
      AccountId::FromUserEmailGaiaId("user1@invalid.domain", "9012345678");

 protected:
  std::unique_ptr<WallpaperControllerClient> wallpaper_controller_client_;
  TestWallpaperController test_wallpaper_controller_;

  content::BrowserTaskEnvironment task_environment_;

  ScopedCrosSettingsTestHelper settings_helper_;
  // local_state_ should be destructed after ProfileManager.
  std::unique_ptr<ScopedTestingLocalState> local_state_;

  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(UserManagerTest, RetrieveTrustedDevicePolicies) {
  SetUserManagerEphemeralUsersEnabled(true);
  SetUserManagerOwnerId(EmptyAccountId());

  SetDeviceSettings(false, owner_account_id_at_invalid_domain_.GetUserEmail(),
                    false);
  RetrieveTrustedDevicePolicies();

  EXPECT_FALSE(GetUserManagerEphemeralUsersEnabled());
  EXPECT_EQ(GetUserManagerOwnerId(), owner_account_id_at_invalid_domain_);
}

TEST_F(UserManagerTest, RemoveUser) {
  std::unique_ptr<MockRemoveUserManager> user_manager =
      CreateMockRemoveUserManager();

  // Create owner account and login in.
  user_manager->UserLoggedIn(owner_account_id_at_invalid_domain_,
                             owner_account_id_at_invalid_domain_.GetUserEmail(),
                             false /* browser_restart */, false /* is_child */);

  // Create non-owner account  and login in.
  user_manager->UserLoggedIn(account_id0_at_invalid_domain_,
                             account_id0_at_invalid_domain_.GetUserEmail(),
                             false /* browser_restart */, false /* is_child */);

  ASSERT_EQ(2U, user_manager->GetUsers().size());

  // Removing logged-in account is unacceptable.
  user_manager->RemoveUser(account_id0_at_invalid_domain_, nullptr);
  EXPECT_EQ(2U, user_manager->GetUsers().size());

  // Recreate the user manager to log out all accounts.
  user_manager = CreateMockRemoveUserManager();
  ASSERT_EQ(2U, user_manager->GetUsers().size());
  ASSERT_EQ(0U, user_manager->GetLoggedInUsers().size());

  // Removing non-owner account is acceptable.
  EXPECT_CALL(*user_manager, AsyncRemoveCryptohome(testing::_)).Times(1);
  UnittestRemoveUserDelegate delegate(account_id0_at_invalid_domain_);
  user_manager->RemoveUser(account_id0_at_invalid_domain_, &delegate);
  EXPECT_TRUE(delegate.HasBeforeUserRemoved());
  EXPECT_TRUE(delegate.HasUserRemoved());
  EXPECT_EQ(1U, user_manager->GetUsers().size());

  // Removing owner account is unacceptable.
  user_manager->RemoveUser(owner_account_id_at_invalid_domain_, nullptr);
  EXPECT_EQ(1U, user_manager->GetUsers().size());
}

TEST_F(UserManagerTest, RemoveAllExceptOwnerFromList) {
  // System salt is needed to remove user wallpaper.
  SystemSaltGetter::Initialize();
  SystemSaltGetter::Get()->SetRawSaltForTesting(
      SystemSaltGetter::RawSalt({1, 2, 3, 4, 5, 6, 7, 8}));

  user_manager::UserManager::Get()->UserLoggedIn(
      owner_account_id_at_invalid_domain_,
      owner_account_id_at_invalid_domain_.GetUserEmail(),
      false /* browser_restart */, false /* is_child */);
  ResetUserManager();
  user_manager::UserManager::Get()->UserLoggedIn(
      account_id0_at_invalid_domain_,
      owner_account_id_at_invalid_domain_.GetUserEmail(),
      false /* browser_restart */, false /* is_child */);
  ResetUserManager();
  user_manager::UserManager::Get()->UserLoggedIn(
      account_id1_at_invalid_domain_,
      owner_account_id_at_invalid_domain_.GetUserEmail(),
      false /* browser_restart */, false /* is_child */);
  ResetUserManager();

  const user_manager::UserList* users =
      &user_manager::UserManager::Get()->GetUsers();
  ASSERT_EQ(3U, users->size());
  EXPECT_EQ((*users)[0]->GetAccountId(), account_id1_at_invalid_domain_);
  EXPECT_EQ((*users)[1]->GetAccountId(), account_id0_at_invalid_domain_);
  EXPECT_EQ((*users)[2]->GetAccountId(), owner_account_id_at_invalid_domain_);

  test_wallpaper_controller_.ClearCounts();
  SetDeviceSettings(true, owner_account_id_at_invalid_domain_.GetUserEmail(),
                    false);
  RetrieveTrustedDevicePolicies();

  users = &user_manager::UserManager::Get()->GetUsers();
  EXPECT_EQ(1U, users->size());
  EXPECT_EQ((*users)[0]->GetAccountId(), owner_account_id_at_invalid_domain_);
  // Verify that the wallpaper is removed when user is removed.
  EXPECT_EQ(2, test_wallpaper_controller_.remove_user_wallpaper_count());
}

TEST_F(UserManagerTest, RegularUserLoggedInAsEphemeral) {
  SetDeviceSettings(true, owner_account_id_at_invalid_domain_.GetUserEmail(),
                    false);
  RetrieveTrustedDevicePolicies();

  user_manager::UserManager::Get()->UserLoggedIn(
      owner_account_id_at_invalid_domain_,
      account_id0_at_invalid_domain_.GetUserEmail(),
      false /* browser_restart */, false /* is_child */);
  ResetUserManager();
  user_manager::UserManager::Get()->UserLoggedIn(
      account_id0_at_invalid_domain_,
      account_id0_at_invalid_domain_.GetUserEmail(),
      false /* browser_restart */, false /* is_child */);
  ResetUserManager();

  const user_manager::UserList* users =
      &user_manager::UserManager::Get()->GetUsers();
  EXPECT_EQ(1U, users->size());
  EXPECT_EQ((*users)[0]->GetAccountId(), owner_account_id_at_invalid_domain_);
}

TEST_F(UserManagerTest, ScreenLockAvailability) {
  // Log in the user and create the profile.
  user_manager::UserManager::Get()->UserLoggedIn(
      owner_account_id_at_invalid_domain_,
      owner_account_id_at_invalid_domain_.GetUserEmail(),
      false /* browser_restart */, false /* is_child */);
  user_manager::User* const user =
      user_manager::UserManager::Get()->GetActiveUser();
  Profile* const profile =
      ProfileHelper::GetProfileByUserIdHashForTest(user->username_hash());

  // Verify that the user is allowed to lock the screen.
  EXPECT_TRUE(user_manager::UserManager::Get()->CanCurrentUserLock());
  EXPECT_EQ(1U, user_manager::UserManager::Get()->GetUnlockUsers().size());

  // The user is not allowed to lock the screen.
  profile->GetPrefs()->SetBoolean(ash::prefs::kAllowScreenLock, false);
  EXPECT_FALSE(user_manager::UserManager::Get()->CanCurrentUserLock());
  EXPECT_EQ(0U, user_manager::UserManager::Get()->GetUnlockUsers().size());

  ResetUserManager();
}

TEST_F(UserManagerTest, ProfileRequiresPolicyUnknown) {
  user_manager::UserManager::Get()->UserLoggedIn(
      owner_account_id_at_invalid_domain_,
      owner_account_id_at_invalid_domain_.GetUserEmail(), false, false);
  EXPECT_EQ(user_manager::known_user::ProfileRequiresPolicy::kUnknown,
            user_manager::known_user::GetProfileRequiresPolicy(
                owner_account_id_at_invalid_domain_));
  ResetUserManager();
}

}  // namespace chromeos
