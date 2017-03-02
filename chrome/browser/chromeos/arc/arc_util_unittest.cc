// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/arc_util.h"

#include <memory>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/test/scoped_command_line.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/supervised/supervised_user_creation_flow.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/browser/chromeos/login/users/wallpaper/wallpaper_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace util {

namespace {

constexpr char kTestProfileName[] = "user@gmail.com";
constexpr char kTestGaiaId[] = "1234567890";

class ScopedLogIn {
 public:
  ScopedLogIn(
      chromeos::FakeChromeUserManager* fake_user_manager,
      const AccountId& account_id,
      user_manager::UserType user_type = user_manager::USER_TYPE_REGULAR)
      : fake_user_manager_(fake_user_manager), account_id_(account_id) {
    switch (user_type) {
      case user_manager::USER_TYPE_REGULAR:  // fallthrough
      case user_manager::USER_TYPE_ACTIVE_DIRECTORY:
        LogIn();
        break;
      case user_manager::USER_TYPE_PUBLIC_ACCOUNT:
        LogInAsPublicAccount();
        break;
      case user_manager::USER_TYPE_ARC_KIOSK_APP:
        LogInArcKioskApp();
        break;
      default:
        NOTREACHED();
    }
  }

  ~ScopedLogIn() { LogOut(); }

 private:
  void LogIn() {
    fake_user_manager_->AddUser(account_id_);
    fake_user_manager_->LoginUser(account_id_);
  }

  void LogInAsPublicAccount() {
    fake_user_manager_->AddPublicAccountUser(account_id_);
    fake_user_manager_->LoginUser(account_id_);
  }

  void LogInArcKioskApp() {
    fake_user_manager_->AddArcKioskAppUser(account_id_);
    fake_user_manager_->LoginUser(account_id_);
  }

  void LogOut() { fake_user_manager_->RemoveUserFromList(account_id_); }

  chromeos::FakeChromeUserManager* fake_user_manager_;
  const AccountId account_id_;

  DISALLOW_COPY_AND_ASSIGN(ScopedLogIn);
};

}  // namespace

class ChromeArcUtilTest : public testing::Test {
 public:
  ChromeArcUtilTest() = default;
  ~ChromeArcUtilTest() override = default;

  void SetUp() override {
    command_line_ = base::MakeUnique<base::test::ScopedCommandLine>();
    command_line_->GetProcessCommandLine()->InitFromArgv({"", "--enable-arc"});

    user_manager_enabler_ =
        base::MakeUnique<chromeos::ScopedUserManagerEnabler>(
            new chromeos::FakeChromeUserManager());
    chromeos::WallpaperManager::Initialize();
    profile_ = base::MakeUnique<TestingProfile>();
    profile_->set_profile_name(kTestProfileName);
  }

  void TearDown() override {
    profile_.reset();
    chromeos::WallpaperManager::Shutdown();
    user_manager_enabler_.reset();
    command_line_.reset();
  }

  TestingProfile* profile() { return profile_.get(); }

  chromeos::FakeChromeUserManager* GetFakeUserManager() const {
    return static_cast<chromeos::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

  void LogIn() {
    const auto account_id = AccountId::FromUserEmailGaiaId(
        profile()->GetProfileUserName(), kTestGaiaId);
    GetFakeUserManager()->AddUser(account_id);
    GetFakeUserManager()->LoginUser(account_id);
  }

 private:
  std::unique_ptr<base::test::ScopedCommandLine> command_line_;
  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<chromeos::ScopedUserManagerEnabler> user_manager_enabler_;
  std::unique_ptr<TestingProfile> profile_;

  DISALLOW_COPY_AND_ASSIGN(ChromeArcUtilTest);
};

TEST_F(ChromeArcUtilTest, IsArcAllowedForProfile) {
  ScopedLogIn login(GetFakeUserManager(),
                    AccountId::FromUserEmailGaiaId(
                        profile()->GetProfileUserName(), kTestGaiaId));
  EXPECT_TRUE(IsArcAllowedForProfile(profile()));

  // false for nullptr.
  EXPECT_FALSE(IsArcAllowedForProfile(nullptr));

  // false for incognito mode profile.
  EXPECT_FALSE(IsArcAllowedForProfile(profile()->GetOffTheRecordProfile()));

  // false for Legacy supervised user.
  profile()->SetSupervisedUserId("foo");
  EXPECT_FALSE(IsArcAllowedForProfile(profile()));
}

TEST_F(ChromeArcUtilTest, IsArcAllowedForProfile_DisableArc) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv({""});
  ScopedLogIn login(GetFakeUserManager(),
                    AccountId::FromUserEmailGaiaId(
                        profile()->GetProfileUserName(), kTestGaiaId));
  EXPECT_FALSE(IsArcAllowedForProfile(profile()));
}

TEST_F(ChromeArcUtilTest, IsArcAllowedForProfile_NonPrimaryProfile) {
  ScopedLogIn login2(
      GetFakeUserManager(),
      AccountId::FromUserEmailGaiaId("user2@gmail.com", "0123456789"));
  ScopedLogIn login(GetFakeUserManager(),
                    AccountId::FromUserEmailGaiaId(
                        profile()->GetProfileUserName(), kTestGaiaId));
  EXPECT_FALSE(IsArcAllowedForProfile(profile()));
}

// User without GAIA account.
TEST_F(ChromeArcUtilTest, IsArcAllowedForProfile_PublicAccount) {
  ScopedLogIn login(GetFakeUserManager(),
                    AccountId::FromUserEmail("public_user@gmail.com"),
                    user_manager::USER_TYPE_PUBLIC_ACCOUNT);
  EXPECT_FALSE(IsArcAllowedForProfile(profile()));
}

TEST_F(ChromeArcUtilTest, IsArcAllowedForProfile_ActiveDirectoryEnabled) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported-with-active-directory"});
  ScopedLogIn login(
      GetFakeUserManager(),
      AccountId::AdFromObjGuid("f04557de-5da2-40ce-ae9d-b8874d8da96e"),
      user_manager::USER_TYPE_ACTIVE_DIRECTORY);
  EXPECT_FALSE(chromeos::ProfileHelper::Get()
                   ->GetUserByProfile(profile())
                   ->HasGaiaAccount());
  EXPECT_TRUE(IsArcAllowedForProfile(profile()));
}

TEST_F(ChromeArcUtilTest, IsArcAllowedForProfile_ActiveDirectoryDisabled) {
  ScopedLogIn login(
      GetFakeUserManager(),
      AccountId::AdFromObjGuid("f04557de-5da2-40ce-ae9d-b8874d8da96e"),
      user_manager::USER_TYPE_ACTIVE_DIRECTORY);
  EXPECT_FALSE(chromeos::ProfileHelper::Get()
                   ->GetUserByProfile(profile())
                   ->HasGaiaAccount());
  EXPECT_FALSE(IsArcAllowedForProfile(profile()));
}

TEST_F(ChromeArcUtilTest, IsArcAllowedForProfile_KioskArcNotAvailable) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv({""});
  ScopedLogIn login(GetFakeUserManager(),
                    AccountId::FromUserEmail(profile()->GetProfileUserName()),
                    user_manager::USER_TYPE_ARC_KIOSK_APP);
  EXPECT_FALSE(chromeos::ProfileHelper::Get()
                   ->GetUserByProfile(profile())
                   ->HasGaiaAccount());
  EXPECT_FALSE(IsArcAllowedForProfile(profile()));
}

TEST_F(ChromeArcUtilTest, IsArcAllowedForProfile_KioskArcInstalled) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=installed"});
  ScopedLogIn login(GetFakeUserManager(),
                    AccountId::FromUserEmail(profile()->GetProfileUserName()),
                    user_manager::USER_TYPE_ARC_KIOSK_APP);
  EXPECT_FALSE(chromeos::ProfileHelper::Get()
                   ->GetUserByProfile(profile())
                   ->HasGaiaAccount());
  EXPECT_TRUE(IsArcAllowedForProfile(profile()));
}

TEST_F(ChromeArcUtilTest, IsArcAllowedForProfile_KioskArcSupported) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--arc-availability=officially-supported"});
  ScopedLogIn login(GetFakeUserManager(),
                    AccountId::FromUserEmail(profile()->GetProfileUserName()),
                    user_manager::USER_TYPE_ARC_KIOSK_APP);
  EXPECT_FALSE(chromeos::ProfileHelper::Get()
                   ->GetUserByProfile(profile())
                   ->HasGaiaAccount());
  EXPECT_TRUE(IsArcAllowedForProfile(profile()));
}

TEST_F(ChromeArcUtilTest, IsArcAllowedForProfile_SupervisedUserFlow) {
  auto manager_id = AccountId::FromUserEmailGaiaId(
      profile()->GetProfileUserName(), kTestGaiaId);
  ScopedLogIn login(GetFakeUserManager(), manager_id);
  GetFakeUserManager()->SetUserFlow(
      manager_id, new chromeos::SupervisedUserCreationFlow(manager_id));
  EXPECT_FALSE(IsArcAllowedForProfile(profile()));
  GetFakeUserManager()->ResetUserFlow(manager_id);
}

// Guest account is interpreted as EphemeralDataUser.
TEST_F(ChromeArcUtilTest, IsArcAllowedForProfile_GuestAccount) {
  ScopedLogIn login(GetFakeUserManager(),
                    GetFakeUserManager()->GetGuestAccountId());
  EXPECT_FALSE(IsArcAllowedForProfile(profile()));
}

// Demo account is interpreted as EphemeralDataUser.
TEST_F(ChromeArcUtilTest, IsArcAllowedForProfile_DemoAccount) {
  ScopedLogIn login(GetFakeUserManager(), user_manager::DemoAccountId());
  EXPECT_FALSE(IsArcAllowedForProfile(profile()));
}

TEST_F(ChromeArcUtilTest, ArcPlayStoreEnabledForProfile) {
  // Ensure IsAllowedForProfile() true.
  ScopedLogIn login(GetFakeUserManager(),
                    AccountId::FromUserEmailGaiaId(
                        profile()->GetProfileUserName(), kTestGaiaId));
  ASSERT_TRUE(IsArcAllowedForProfile(profile()));

  // By default, Google Play Store is disabled.
  EXPECT_FALSE(IsArcPlayStoreEnabledForProfile(profile()));

  // Enable Google Play Store.
  SetArcPlayStoreEnabledForProfile(profile(), true);
  EXPECT_TRUE(IsArcPlayStoreEnabledForProfile(profile()));

  // Disable Google Play Store.
  SetArcPlayStoreEnabledForProfile(profile(), false);
  EXPECT_FALSE(IsArcPlayStoreEnabledForProfile(profile()));
}

TEST_F(ChromeArcUtilTest, ArcPlayStoreEnabledForProfile_NotAllowed) {
  ASSERT_FALSE(IsArcAllowedForProfile(profile()));

  // If ARC is not allowed for the profile, always return false.
  EXPECT_FALSE(IsArcPlayStoreEnabledForProfile(profile()));

  // Directly set the preference value, to avoid DCHECK in
  // SetArcPlayStoreEnabledForProfile().
  profile()->GetPrefs()->SetBoolean(prefs::kArcEnabled, true);
  EXPECT_FALSE(IsArcPlayStoreEnabledForProfile(profile()));
}

TEST_F(ChromeArcUtilTest, ArcPlayStoreEnabledForProfile_Managed) {
  // Ensure IsAllowedForProfile() true.
  ScopedLogIn login(GetFakeUserManager(),
                    AccountId::FromUserEmailGaiaId(
                        profile()->GetProfileUserName(), kTestGaiaId));
  ASSERT_TRUE(IsArcAllowedForProfile(profile()));

  // By default it is not managed.
  EXPECT_FALSE(IsArcPlayStoreEnabledPreferenceManagedForProfile(profile()));
  EXPECT_FALSE(IsArcPlayStoreEnabledForProfile(profile()));

  // 1) Set managed preference to true, then try to set the value to false
  // via SetArcPlayStoreEnabledForProfile().
  profile()->GetTestingPrefService()->SetManagedPref(prefs::kArcEnabled,
                                                     new base::Value(true));
  EXPECT_TRUE(IsArcPlayStoreEnabledPreferenceManagedForProfile(profile()));
  EXPECT_TRUE(IsArcPlayStoreEnabledForProfile(profile()));
  SetArcPlayStoreEnabledForProfile(profile(), false);
  EXPECT_TRUE(IsArcPlayStoreEnabledPreferenceManagedForProfile(profile()));
  EXPECT_TRUE(IsArcPlayStoreEnabledForProfile(profile()));

  // Remove managed state.
  profile()->GetTestingPrefService()->RemoveManagedPref(prefs::kArcEnabled);
  EXPECT_FALSE(IsArcPlayStoreEnabledPreferenceManagedForProfile(profile()));

  // 2) Set managed preference to false, then try to set the value to true
  // via SetArcPlayStoreEnabledForProfile().
  profile()->GetTestingPrefService()->SetManagedPref(prefs::kArcEnabled,
                                                     new base::Value(false));
  EXPECT_TRUE(IsArcPlayStoreEnabledPreferenceManagedForProfile(profile()));
  EXPECT_FALSE(IsArcPlayStoreEnabledForProfile(profile()));
  SetArcPlayStoreEnabledForProfile(profile(), true);
  EXPECT_TRUE(IsArcPlayStoreEnabledPreferenceManagedForProfile(profile()));
  EXPECT_FALSE(IsArcPlayStoreEnabledForProfile(profile()));

  // Remove managed state.
  profile()->GetTestingPrefService()->RemoveManagedPref(prefs::kArcEnabled);
  EXPECT_FALSE(IsArcPlayStoreEnabledPreferenceManagedForProfile(profile()));
}

}  // namespace util
}  // namespace arc
