// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdlib>
#include <cstring>

#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_service.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager_impl.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/chromeos/settings/stub_cros_settings_provider.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/settings/cros_settings_provider.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

class UnittestProfileManager : public ::ProfileManagerWithoutInit {
 public:
  explicit UnittestProfileManager(const base::FilePath& user_data_dir)
      : ::ProfileManagerWithoutInit(user_data_dir) {}

 protected:
  virtual Profile* CreateProfileHelper(
      const base::FilePath& file_path) OVERRIDE {
    if (!base::PathExists(file_path)) {
      if (!base::CreateDirectory(file_path))
        return NULL;
    }
    return new TestingProfile(file_path, NULL);
  }
};


class UserManagerTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    CommandLine& command_line = *CommandLine::ForCurrentProcess();
    command_line.AppendSwitch(::switches::kTestType);
    command_line.AppendSwitch(
        chromeos::switches::kIgnoreUserProfileMappingForTests);

    cros_settings_ = CrosSettings::Get();

    // Replace the real DeviceSettingsProvider with a stub.
    device_settings_provider_ =
        cros_settings_->GetProvider(chromeos::kReportDeviceVersionInfo);
    EXPECT_TRUE(device_settings_provider_);
    EXPECT_TRUE(
        cros_settings_->RemoveSettingsProvider(device_settings_provider_));
    cros_settings_->AddSettingsProvider(&stub_settings_provider_);

    // Populate the stub DeviceSettingsProvider with valid values.
    SetDeviceSettings(false, "", false);

    // Register an in-memory local settings instance.
    local_state_.reset(
        new ScopedTestingLocalState(TestingBrowserProcess::GetGlobal()));

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    TestingBrowserProcess::GetGlobal()->SetProfileManager(
        new UnittestProfileManager(temp_dir_.path()));

    chromeos::DBusThreadManager::Initialize();

    ResetUserManager();
  }

  virtual void TearDown() OVERRIDE {
    // Unregister the in-memory local settings instance.
    local_state_.reset();

    // Restore the real DeviceSettingsProvider.
    EXPECT_TRUE(
      cros_settings_->RemoveSettingsProvider(&stub_settings_provider_));
    cros_settings_->AddSettingsProvider(device_settings_provider_);

    // Shut down the DeviceSettingsService.
    DeviceSettingsService::Get()->UnsetSessionManager();
    TestingBrowserProcess::GetGlobal()->SetProfileManager(NULL);

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

  const std::string& GetUserManagerOwnerEmail() const {
    return GetChromeUserManager()->GetOwnerEmail();
  }

  void SetUserManagerOwnerEmail(const std::string& owner_email) {
    GetChromeUserManager()->SetOwnerEmail(owner_email);
  }

  void ResetUserManager() {
    // Reset the UserManager singleton.
    user_manager_enabler_.reset();
    // Initialize the UserManager singleton to a fresh ChromeUserManagerImpl
    // instance.
    user_manager_enabler_.reset(
        new ScopedUserManagerEnabler(new ChromeUserManagerImpl));

    // ChromeUserManagerImpl ctor posts a task to reload policies.
    base::RunLoop().RunUntilIdle();
  }

  void SetDeviceSettings(bool ephemeral_users_enabled,
                         const std::string &owner,
                         bool supervised_users_enabled) {
    base::FundamentalValue
        ephemeral_users_enabled_value(ephemeral_users_enabled);
    stub_settings_provider_.Set(kAccountsPrefEphemeralUsersEnabled,
        ephemeral_users_enabled_value);
    base::StringValue owner_value(owner);
    stub_settings_provider_.Set(kDeviceOwner, owner_value);
    stub_settings_provider_.Set(kAccountsPrefSupervisedUsersEnabled,
        base::FundamentalValue(supervised_users_enabled));
  }

  void RetrieveTrustedDevicePolicies() {
    GetChromeUserManager()->RetrieveTrustedDevicePolicies();
  }

 protected:
  content::TestBrowserThreadBundle thread_bundle_;

  CrosSettings* cros_settings_;
  CrosSettingsProvider* device_settings_provider_;
  StubCrosSettingsProvider stub_settings_provider_;
  scoped_ptr<ScopedTestingLocalState> local_state_;

  ScopedTestDeviceSettingsService test_device_settings_service_;
  ScopedTestCrosSettings test_cros_settings_;

  scoped_ptr<ScopedUserManagerEnabler> user_manager_enabler_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(UserManagerTest, RetrieveTrustedDevicePolicies) {
  SetUserManagerEphemeralUsersEnabled(true);
  SetUserManagerOwnerEmail("");

  SetDeviceSettings(false, "owner@invalid.domain", false);
  RetrieveTrustedDevicePolicies();

  EXPECT_FALSE(GetUserManagerEphemeralUsersEnabled());
  EXPECT_EQ(GetUserManagerOwnerEmail(), "owner@invalid.domain");
}

TEST_F(UserManagerTest, RemoveAllExceptOwnerFromList) {
  user_manager::UserManager::Get()->UserLoggedIn(
      "owner@invalid.domain", "owner@invalid.domain", false);
  ResetUserManager();
  user_manager::UserManager::Get()->UserLoggedIn(
      "user0@invalid.domain", "owner@invalid.domain", false);
  ResetUserManager();
  user_manager::UserManager::Get()->UserLoggedIn(
      "user1@invalid.domain", "owner@invalid.domain", false);
  ResetUserManager();

  const user_manager::UserList* users =
      &user_manager::UserManager::Get()->GetUsers();
  ASSERT_EQ(3U, users->size());
  EXPECT_EQ((*users)[0]->email(), "user1@invalid.domain");
  EXPECT_EQ((*users)[1]->email(), "user0@invalid.domain");
  EXPECT_EQ((*users)[2]->email(), "owner@invalid.domain");

  SetDeviceSettings(true, "owner@invalid.domain", false);
  RetrieveTrustedDevicePolicies();

  users = &user_manager::UserManager::Get()->GetUsers();
  EXPECT_EQ(1U, users->size());
  EXPECT_EQ((*users)[0]->email(), "owner@invalid.domain");
}

TEST_F(UserManagerTest, RegularUserLoggedInAsEphemeral) {
  SetDeviceSettings(true, "owner@invalid.domain", false);
  RetrieveTrustedDevicePolicies();

  user_manager::UserManager::Get()->UserLoggedIn(
      "owner@invalid.domain", "user0@invalid.domain", false);
  ResetUserManager();
  user_manager::UserManager::Get()->UserLoggedIn(
      "user0@invalid.domain", "user0@invalid.domain", false);
  ResetUserManager();

  const user_manager::UserList* users =
      &user_manager::UserManager::Get()->GetUsers();
  EXPECT_EQ(1U, users->size());
  EXPECT_EQ((*users)[0]->email(), "owner@invalid.domain");
}

}  // namespace chromeos
