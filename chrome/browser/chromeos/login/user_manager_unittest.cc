// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdlib>
#include <cstring>

#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_service.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/user_manager_impl.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/cros_settings_names.h"
#include "chrome/browser/chromeos/settings/cros_settings_provider.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/chromeos/settings/stub_cros_settings_provider.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

class UserManagerTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
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

    base::RunLoop().RunUntilIdle();
  }

  UserManagerImpl* GetUserManagerImpl() const {
    return static_cast<UserManagerImpl*>(UserManager::Get());
  }

  bool GetUserManagerEphemeralUsersEnabled() const {
    return GetUserManagerImpl()->ephemeral_users_enabled_;
  }

  void SetUserManagerEphemeralUsersEnabled(bool ephemeral_users_enabled) {
    GetUserManagerImpl()->ephemeral_users_enabled_ = ephemeral_users_enabled;
  }

  const std::string& GetUserManagerOwnerEmail() const {
    return GetUserManagerImpl()-> owner_email_;
  }

  void SetUserManagerOwnerEmail(const std::string& owner_email) {
    GetUserManagerImpl()->owner_email_ = owner_email;
  }

  void ResetUserManager() {
    // Reset the UserManager singleton.
    user_manager_enabler_.reset();
    // Initialize the UserManager singleton to a fresh UserManagerImpl instance.
    user_manager_enabler_.reset(
        new ScopedUserManagerEnabler(new UserManagerImpl));
  }

  void SetDeviceSettings(bool ephemeral_users_enabled,
                         const std::string &owner,
                         bool locally_managed_users_enabled) {
    base::FundamentalValue
        ephemeral_users_enabled_value(ephemeral_users_enabled);
    stub_settings_provider_.Set(kAccountsPrefEphemeralUsersEnabled,
        ephemeral_users_enabled_value);
    base::StringValue owner_value(owner);
    stub_settings_provider_.Set(kDeviceOwner, owner_value);
    stub_settings_provider_.Set(kAccountsPrefSupervisedUsersEnabled,
        base::FundamentalValue(locally_managed_users_enabled));
  }

  void RetrieveTrustedDevicePolicies() {
    GetUserManagerImpl()->RetrieveTrustedDevicePolicies();
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
  UserManager::Get()->UserLoggedIn(
      "owner@invalid.domain", "owner@invalid.domain", false);
  ResetUserManager();
  UserManager::Get()->UserLoggedIn(
      "user0@invalid.domain", "owner@invalid.domain", false);
  ResetUserManager();
  UserManager::Get()->UserLoggedIn(
      "user1@invalid.domain", "owner@invalid.domain", false);
  ResetUserManager();

  const UserList* users = &UserManager::Get()->GetUsers();
  ASSERT_EQ(3U, users->size());
  EXPECT_EQ((*users)[0]->email(), "user1@invalid.domain");
  EXPECT_EQ((*users)[1]->email(), "user0@invalid.domain");
  EXPECT_EQ((*users)[2]->email(), "owner@invalid.domain");

  SetDeviceSettings(true, "owner@invalid.domain", false);
  RetrieveTrustedDevicePolicies();

  users = &UserManager::Get()->GetUsers();
  EXPECT_EQ(1U, users->size());
  EXPECT_EQ((*users)[0]->email(), "owner@invalid.domain");
}

TEST_F(UserManagerTest, RegularUserLoggedInAsEphemeral) {
  SetDeviceSettings(true, "owner@invalid.domain", false);
  RetrieveTrustedDevicePolicies();

  UserManager::Get()->UserLoggedIn(
      "owner@invalid.domain", "user0@invalid.domain", false);
  ResetUserManager();
  UserManager::Get()->UserLoggedIn(
      "user0@invalid.domain", "user0@invalid.domain", false);
  ResetUserManager();

  const UserList* users = &UserManager::Get()->GetUsers();
  EXPECT_EQ(1U, users->size());
  EXPECT_EQ((*users)[0]->email(), "owner@invalid.domain");
}

}  // namespace chromeos
