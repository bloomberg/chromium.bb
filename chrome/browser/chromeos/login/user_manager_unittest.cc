// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdlib>
#include <cstring>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/testing_pref_service.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/mock_cert_library.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/user_manager_impl.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/cros_settings_names.h"
#include "chrome/browser/chromeos/settings/cros_settings_provider.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/chromeos/settings/stub_cros_settings_provider.h"
#include "chrome/test/base/testing_browser_process.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AnyNumber;

namespace chromeos {

class UserManagerTest : public testing::Test {
 public:
  UserManagerTest()
      : message_loop_(MessageLoop::TYPE_UI),
        ui_thread_(content::BrowserThread::UI, &message_loop_),
        file_thread_(content::BrowserThread::FILE, &message_loop_) {
  }

  virtual void SetUp() OVERRIDE {
    MockCertLibrary* mock_cert_library = new MockCertLibrary();
    EXPECT_CALL(*mock_cert_library, LoadKeyStore()).Times(AnyNumber());
    chromeos::CrosLibrary::Get()->GetTestApi()->SetCertLibrary(
        mock_cert_library, true);

    cros_settings_ = CrosSettings::Get();

    // Replace the real DeviceSettingsProvider with a stub.
    device_settings_provider_ =
        cros_settings_->GetProvider(chromeos::kReportDeviceVersionInfo);
    EXPECT_TRUE(device_settings_provider_);
    EXPECT_TRUE(
        cros_settings_->RemoveSettingsProvider(device_settings_provider_));
    cros_settings_->AddSettingsProvider(&stub_settings_provider_);

    // Populate the stub DeviceSettingsProvider with valid values.
    SetDeviceSettings(false, "");

    // Register an in-memory local settings instance.
    local_state_.reset(new TestingPrefServiceSimple);
    reinterpret_cast<TestingBrowserProcess*>(g_browser_process)
        ->SetLocalState(local_state_.get());
    UserManager::RegisterPrefs(local_state_->registry());
    // Wallpaper manager and user image managers prefs will be accessed by the
    // unit-test as well.
    UserImageManager::RegisterPrefs(local_state_->registry());
    WallpaperManager::RegisterPrefs(local_state_->registry());

    old_user_manager_ = UserManager::Get();
    ResetUserManager();
  }

  virtual void TearDown() OVERRIDE {
    // Unregister the in-memory local settings instance.
    reinterpret_cast<TestingBrowserProcess*>(g_browser_process)
        ->SetLocalState(0);

    // Restore the real DeviceSettingsProvider.
    EXPECT_TRUE(
      cros_settings_->RemoveSettingsProvider(&stub_settings_provider_));
    cros_settings_->AddSettingsProvider(device_settings_provider_);

    UserManager::Set(old_user_manager_);

    // Shut down the DeviceSettingsService.
    DeviceSettingsService::Get()->Shutdown();

    // Shut down the remaining UserManager instances.
    if (user_manager_impl)
      user_manager_impl->Shutdown();
    UserManager::Get()->Shutdown();

    base::RunLoop().RunUntilIdle();
  }

  bool GetUserManagerEphemeralUsersEnabled() const {
    return reinterpret_cast<UserManagerImpl*>(UserManager::Get())->
        ephemeral_users_enabled_;
  }

  void SetUserManagerEphemeralUsersEnabled(bool ephemeral_users_enabled) {
    reinterpret_cast<UserManagerImpl*>(UserManager::Get())->
        ephemeral_users_enabled_ = ephemeral_users_enabled;
  }

  const std::string& GetUserManagerOwnerEmail() const {
    return reinterpret_cast<UserManagerImpl*>(UserManager::Get())->
        owner_email_;
  }

  void SetUserManagerOwnerEmail(const std::string& owner_email) {
    reinterpret_cast<UserManagerImpl*>(UserManager::Get())->
        owner_email_ = owner_email;
  }

  void ResetUserManager() {
    if (user_manager_impl)
      user_manager_impl->Shutdown();
    user_manager_impl.reset(new UserManagerImpl());
    UserManager::Set(user_manager_impl.get());
  }

  void SetDeviceSettings(bool ephemeral_users_enabled,
                         const std::string &owner) {
    base::FundamentalValue
        ephemeral_users_enabled_value(ephemeral_users_enabled);
    stub_settings_provider_.Set(kAccountsPrefEphemeralUsersEnabled,
        ephemeral_users_enabled_value);
    base::StringValue owner_value(owner);
    stub_settings_provider_.Set(kDeviceOwner, owner_value);
  }

  void RetrieveTrustedDevicePolicies() {
    reinterpret_cast<UserManagerImpl*>(UserManager::Get())->
        RetrieveTrustedDevicePolicies();
  }

 protected:
  MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

  CrosSettings* cros_settings_;
  CrosSettingsProvider* device_settings_provider_;
  StubCrosSettingsProvider stub_settings_provider_;
  scoped_ptr<TestingPrefServiceSimple> local_state_;

  // Initializes / shuts down a stub CrosLibrary.
  chromeos::ScopedStubCrosEnabler stub_cros_enabler_;

  scoped_ptr<UserManagerImpl> user_manager_impl;
  UserManager* old_user_manager_;
};

TEST_F(UserManagerTest, RetrieveTrustedDevicePolicies) {
  SetUserManagerEphemeralUsersEnabled(true);
  SetUserManagerOwnerEmail("");

  SetDeviceSettings(false, "owner@invalid.domain");
  RetrieveTrustedDevicePolicies();

  EXPECT_FALSE(GetUserManagerEphemeralUsersEnabled());
  EXPECT_EQ(GetUserManagerOwnerEmail(), "owner@invalid.domain");
}

TEST_F(UserManagerTest, RemoveAllExceptOwnerFromList) {
  UserManager::Get()->UserLoggedIn("owner@invalid.domain", false);
  ResetUserManager();
  UserManager::Get()->UserLoggedIn("user0@invalid.domain", false);
  ResetUserManager();
  UserManager::Get()->UserLoggedIn("user1@invalid.domain", false);
  ResetUserManager();

  const UserList* users = &UserManager::Get()->GetUsers();
  ASSERT_EQ(3U, users->size());
  EXPECT_EQ((*users)[0]->email(), "user1@invalid.domain");
  EXPECT_EQ((*users)[1]->email(), "user0@invalid.domain");
  EXPECT_EQ((*users)[2]->email(), "owner@invalid.domain");

  SetDeviceSettings(true, "owner@invalid.domain");
  RetrieveTrustedDevicePolicies();

  users = &UserManager::Get()->GetUsers();
  EXPECT_EQ(1U, users->size());
  EXPECT_EQ((*users)[0]->email(), "owner@invalid.domain");
}

TEST_F(UserManagerTest, RegularUserLoggedInAsEphemeral) {
  SetDeviceSettings(true, "owner@invalid.domain");
  RetrieveTrustedDevicePolicies();

  UserManager::Get()->UserLoggedIn("owner@invalid.domain", false);
  ResetUserManager();
  UserManager::Get()->UserLoggedIn("user0@invalid.domain", false);
  ResetUserManager();

  const UserList* users = &UserManager::Get()->GetUsers();
  EXPECT_EQ(1U, users->size());
  EXPECT_EQ((*users)[0]->email(), "owner@invalid.domain");
}

}  // namespace chromeos
