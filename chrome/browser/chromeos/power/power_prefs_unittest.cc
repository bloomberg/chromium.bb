// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/power_prefs.h"

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/dbus/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/policy.pb.h"
#include "chromeos/dbus/power_policy_controller.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/syncable_prefs/pref_service_syncable.h"
#include "components/syncable_prefs/testing_pref_service_syncable.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

class PowerPrefsTest : public testing::Test {
 protected:
  // Screen lock state that determines which delays are used by
  // GetExpectedPowerPolicyForProfile().
  enum ScreenLockState {
    LOCKED,
    UNLOCKED,
  };

  PowerPrefsTest();

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  const Profile* GetProfile() const;

  std::string GetExpectedPowerPolicyForProfile(
      Profile* profile,
      ScreenLockState screen_lock_state) const;
  std::string GetCurrentPowerPolicy() const;
  bool GetExpectedAllowScreenWakeLocksForProfile(Profile* profile) const;
  bool GetCurrentAllowScreenWakeLocks() const;

  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfileManager profile_manager_;
  PowerPolicyController* power_policy_controller_;     // Not owned.
  scoped_ptr<FakePowerManagerClient> fake_power_manager_client_;

  scoped_ptr<PowerPrefs> power_prefs_;

  DISALLOW_COPY_AND_ASSIGN(PowerPrefsTest);
};

PowerPrefsTest::PowerPrefsTest()
    : profile_manager_(TestingBrowserProcess::GetGlobal()),
      power_policy_controller_(NULL),
      fake_power_manager_client_(new FakePowerManagerClient) {
}

void PowerPrefsTest::SetUp() {
  testing::Test::SetUp();

  PowerPolicyController::Initialize(fake_power_manager_client_.get());
  power_policy_controller_ = PowerPolicyController::Get();

  ASSERT_TRUE(profile_manager_.SetUp());

  power_prefs_.reset(new PowerPrefs(power_policy_controller_));
  EXPECT_FALSE(GetProfile());
  EXPECT_EQ(PowerPolicyController::GetPolicyDebugString(
                power_manager::PowerManagementPolicy()),
            GetCurrentPowerPolicy());
}

void PowerPrefsTest::TearDown() {
  power_prefs_.reset();
  PowerPolicyController::Shutdown();
  testing::Test::TearDown();
}

const Profile* PowerPrefsTest::GetProfile() const {
  return power_prefs_->profile_;
}

std::string PowerPrefsTest::GetExpectedPowerPolicyForProfile(
    Profile* profile,
    ScreenLockState screen_lock_state) const {
  const PrefService* prefs = profile->GetPrefs();
  power_manager::PowerManagementPolicy expected_policy;
  expected_policy.mutable_ac_delays()->set_screen_dim_ms(prefs->GetInteger(
      screen_lock_state == LOCKED ? prefs::kPowerLockScreenDimDelayMs
                                  : prefs::kPowerAcScreenDimDelayMs));
  expected_policy.mutable_ac_delays()->set_screen_off_ms(prefs->GetInteger(
      screen_lock_state == LOCKED ? prefs::kPowerLockScreenOffDelayMs
                                  : prefs::kPowerAcScreenOffDelayMs));
  expected_policy.mutable_ac_delays()->set_screen_lock_ms(
      prefs->GetInteger(prefs::kPowerAcScreenLockDelayMs));
  expected_policy.mutable_ac_delays()->set_idle_warning_ms(
      prefs->GetInteger(prefs::kPowerAcIdleWarningDelayMs));
  expected_policy.mutable_ac_delays()->set_idle_ms(
      prefs->GetInteger(prefs::kPowerAcIdleDelayMs));
  expected_policy.mutable_battery_delays()->set_screen_dim_ms(prefs->GetInteger(
      screen_lock_state == LOCKED ? prefs::kPowerLockScreenDimDelayMs
                                  : prefs::kPowerBatteryScreenDimDelayMs));
  expected_policy.mutable_battery_delays()->set_screen_off_ms(prefs->GetInteger(
      screen_lock_state == LOCKED ? prefs::kPowerLockScreenOffDelayMs
                                  : prefs::kPowerBatteryScreenOffDelayMs));
  expected_policy.mutable_battery_delays()->set_screen_lock_ms(
      prefs->GetInteger(prefs::kPowerBatteryScreenLockDelayMs));
  expected_policy.mutable_battery_delays()->set_idle_warning_ms(
      prefs->GetInteger(prefs::kPowerBatteryIdleWarningDelayMs));
  expected_policy.mutable_battery_delays()->set_idle_ms(
      prefs->GetInteger(prefs::kPowerBatteryIdleDelayMs));
  expected_policy.set_ac_idle_action(
      static_cast<power_manager::PowerManagementPolicy_Action>(
            prefs->GetInteger(prefs::kPowerAcIdleAction)));
  expected_policy.set_battery_idle_action(
      static_cast<power_manager::PowerManagementPolicy_Action>(
            prefs->GetInteger(prefs::kPowerBatteryIdleAction)));
  expected_policy.set_lid_closed_action(
      static_cast<power_manager::PowerManagementPolicy_Action>(
            prefs->GetInteger(prefs::kPowerLidClosedAction)));
  expected_policy.set_use_audio_activity(
      prefs->GetBoolean(prefs::kPowerUseAudioActivity));
  expected_policy.set_use_video_activity(
      prefs->GetBoolean(prefs::kPowerUseVideoActivity));
  expected_policy.set_presentation_screen_dim_delay_factor(
      prefs->GetDouble(prefs::kPowerPresentationScreenDimDelayFactor));
  expected_policy.set_user_activity_screen_dim_delay_factor(
      prefs->GetDouble(prefs::kPowerUserActivityScreenDimDelayFactor));
  expected_policy.set_wait_for_initial_user_activity(
      prefs->GetBoolean(prefs::kPowerWaitForInitialUserActivity));
  expected_policy.set_force_nonzero_brightness_for_user_activity(
      prefs->GetBoolean(prefs::kPowerForceNonzeroBrightnessForUserActivity));
  expected_policy.set_reason("Prefs");
  return PowerPolicyController::GetPolicyDebugString(expected_policy);
}

std::string PowerPrefsTest::GetCurrentPowerPolicy() const {
  return PowerPolicyController::GetPolicyDebugString(
      fake_power_manager_client_->policy());
}

bool PowerPrefsTest::GetCurrentAllowScreenWakeLocks() const {
  return power_policy_controller_->honor_screen_wake_locks_;
}

bool PowerPrefsTest::GetExpectedAllowScreenWakeLocksForProfile(
    Profile* profile) const {
  return profile->GetPrefs()->GetBoolean(prefs::kPowerAllowScreenWakeLocks);
}

TEST_F(PowerPrefsTest, LoginScreen) {
  // Set up login profile.
  scoped_ptr<syncable_prefs::TestingPrefServiceSyncable> login_profile_prefs(
      new syncable_prefs::TestingPrefServiceSyncable);
  chrome::RegisterLoginProfilePrefs(login_profile_prefs->registry());
  TestingProfile::Builder builder;
  builder.SetPath(
      profile_manager_.profiles_dir().AppendASCII(chrome::kInitialProfile));
  builder.SetPrefService(std::move(login_profile_prefs));
  TestingProfile* login_profile = builder.BuildIncognito(
      profile_manager_.CreateTestingProfile(chrome::kInitialProfile));

  // Inform power_prefs_ that the login screen is being shown.
  power_prefs_->Observe(chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
                        content::Source<PowerPrefsTest>(this),
                        content::NotificationService::NoDetails());

  EXPECT_EQ(login_profile, GetProfile());
  EXPECT_EQ(GetExpectedPowerPolicyForProfile(login_profile, UNLOCKED),
            GetCurrentPowerPolicy());
  EXPECT_EQ(GetExpectedAllowScreenWakeLocksForProfile(login_profile),
            GetCurrentAllowScreenWakeLocks());

  TestingProfile* other_profile =
      profile_manager_.CreateTestingProfile("other");

  // Inform power_prefs_ that an unrelated profile has been destroyed.
  power_prefs_->Observe(chrome::NOTIFICATION_PROFILE_DESTROYED,
                        content::Source<Profile>(other_profile),
                        content::NotificationService::NoDetails());

  // Verify that the login profile's power prefs are still being used.
  EXPECT_EQ(login_profile, GetProfile());
  EXPECT_EQ(GetExpectedPowerPolicyForProfile(login_profile, UNLOCKED),
            GetCurrentPowerPolicy());
  EXPECT_EQ(GetExpectedAllowScreenWakeLocksForProfile(login_profile),
            GetCurrentAllowScreenWakeLocks());

  // Lock the screen and check that the expected delays are used.
  bool screen_is_locked = true;
  power_prefs_->Observe(chrome::NOTIFICATION_SCREEN_LOCK_STATE_CHANGED,
                        content::Source<Profile>(login_profile),
                        content::Details<bool>(&screen_is_locked));
  EXPECT_EQ(GetExpectedPowerPolicyForProfile(login_profile, LOCKED),
            GetCurrentPowerPolicy());

  // Unlock the screen.
  screen_is_locked = false;
  power_prefs_->Observe(chrome::NOTIFICATION_SCREEN_LOCK_STATE_CHANGED,
                        content::Source<Profile>(login_profile),
                        content::Details<bool>(&screen_is_locked));
  EXPECT_EQ(GetExpectedPowerPolicyForProfile(login_profile, UNLOCKED),
            GetCurrentPowerPolicy());

  // Inform power_prefs_ that the login profile has been destroyed.
  power_prefs_->Observe(chrome::NOTIFICATION_PROFILE_DESTROYED,
                        content::Source<Profile>(login_profile),
                        content::NotificationService::NoDetails());

  // The login profile's prefs should still be used.
  EXPECT_FALSE(GetProfile());
  EXPECT_EQ(GetExpectedPowerPolicyForProfile(login_profile, UNLOCKED),
            GetCurrentPowerPolicy());
}

TEST_F(PowerPrefsTest, UserSession) {
  FakeChromeUserManager* user_manager = new FakeChromeUserManager();
  ScopedUserManagerEnabler user_manager_enabler(user_manager);

  // Set up user profile.
  const char test_user1[] = "test-user1@example.com";
  const AccountId test_account_id1(AccountId::FromUserEmail(test_user1));
  user_manager->AddUser(test_account_id1);
  user_manager->LoginUser(test_account_id1);
  TestingProfile* user_profile =
      profile_manager_.CreateTestingProfile(test_account_id1.GetUserEmail());

  profile_manager_.SetLoggedIn(true);

  // Inform power_prefs_ that a session has started.
  power_prefs_->Observe(chrome::NOTIFICATION_SESSION_STARTED,
                        content::Source<PowerPrefsTest>(this),
                        content::NotificationService::NoDetails());

  EXPECT_EQ(user_profile, GetProfile());
  EXPECT_EQ(GetExpectedPowerPolicyForProfile(user_profile, UNLOCKED),
            GetCurrentPowerPolicy());
  EXPECT_EQ(GetExpectedAllowScreenWakeLocksForProfile(user_profile),
            GetCurrentAllowScreenWakeLocks());

  const char test_user2[] = "test-user2@example.com";
  const AccountId test_account_id2(AccountId::FromUserEmail(test_user2));
  user_manager->AddUser(test_account_id2);
  user_manager->LoginUser(test_account_id2);
  TestingProfile* other_profile =
      profile_manager_.CreateTestingProfile(test_account_id2.GetUserEmail());

  // Inform power_prefs_ that an unrelated profile has been destroyed.
  power_prefs_->Observe(chrome::NOTIFICATION_PROFILE_DESTROYED,
                        content::Source<Profile>(other_profile),
                        content::NotificationService::NoDetails());

  // Verify that the user profile's power prefs are still being used.
  EXPECT_EQ(user_profile, GetProfile());
  EXPECT_EQ(GetExpectedPowerPolicyForProfile(user_profile, UNLOCKED),
            GetCurrentPowerPolicy());
  EXPECT_EQ(GetExpectedAllowScreenWakeLocksForProfile(user_profile),
            GetCurrentAllowScreenWakeLocks());

  // Simulate the login screen coming up as part of screen locking.
  power_prefs_->Observe(chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
                        content::Source<PowerPrefsTest>(this),
                        content::NotificationService::NoDetails());

  // Verify that power policy didn't revert to login screen settings.
  EXPECT_EQ(user_profile, GetProfile());
  EXPECT_EQ(GetExpectedPowerPolicyForProfile(user_profile, UNLOCKED),
            GetCurrentPowerPolicy());
  EXPECT_EQ(GetExpectedAllowScreenWakeLocksForProfile(user_profile),
            GetCurrentAllowScreenWakeLocks());

  // Inform power_prefs_ that the session has ended and the user profile has
  // been destroyed.
  power_prefs_->Observe(chrome::NOTIFICATION_PROFILE_DESTROYED,
                        content::Source<Profile>(user_profile),
                        content::NotificationService::NoDetails());

  // The user profile's prefs should still be used.
  EXPECT_FALSE(GetProfile());
  EXPECT_EQ(GetExpectedPowerPolicyForProfile(user_profile, UNLOCKED),
            GetCurrentPowerPolicy());
}

}  // namespace chromeos
