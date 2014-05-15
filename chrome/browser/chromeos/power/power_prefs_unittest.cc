// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/power_prefs.h"

#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/pref_service_syncable.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/fake_dbus_thread_manager.h"
#include "chromeos/dbus/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/policy.pb.h"
#include "chromeos/dbus/power_policy_controller.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

class PowerPrefsTest : public testing::Test {
 protected:
  PowerPrefsTest();

  // testing::Test:
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  const Profile* GetProfile() const;

  std::string GetExpectedPowerPolicyForProfile(Profile* profile) const;
  std::string GetCurrentPowerPolicy() const;
  bool GetExpectedAllowScreenWakeLocksForProfile(Profile* profile) const;
  bool GetCurrentAllowScreenWakeLocks() const;

  TestingProfileManager profile_manager_;
  FakeDBusThreadManager fake_dbus_thread_manager_;
  PowerPolicyController* power_policy_controller_;     // Not owned.
  FakePowerManagerClient* fake_power_manager_client_;  // Not owned.

  scoped_ptr<PowerPrefs> power_prefs_;

  DISALLOW_COPY_AND_ASSIGN(PowerPrefsTest);
};

PowerPrefsTest::PowerPrefsTest()
    : profile_manager_(TestingBrowserProcess::GetGlobal()),
      power_policy_controller_(new PowerPolicyController),
      fake_power_manager_client_(new FakePowerManagerClient) {
  fake_dbus_thread_manager_.SetPowerManagerClient(
      scoped_ptr<PowerManagerClient>(fake_power_manager_client_));
  fake_dbus_thread_manager_.SetPowerPolicyController(
      scoped_ptr<PowerPolicyController>(power_policy_controller_));
  power_policy_controller_->Init(&fake_dbus_thread_manager_);
}

void PowerPrefsTest::SetUp() {
  testing::Test::SetUp();
  ASSERT_TRUE(profile_manager_.SetUp());

  power_prefs_.reset(new PowerPrefs(power_policy_controller_));
  EXPECT_FALSE(GetProfile());
  EXPECT_EQ(PowerPolicyController::GetPolicyDebugString(
                power_manager::PowerManagementPolicy()),
            GetCurrentPowerPolicy());
}

void PowerPrefsTest::TearDown() {
  power_prefs_.reset();
  testing::Test::TearDown();
}

const Profile* PowerPrefsTest::GetProfile() const {
  return power_prefs_->profile_;
}

std::string PowerPrefsTest::GetExpectedPowerPolicyForProfile(
    Profile* profile) const {
  const PrefService* prefs = profile->GetPrefs();
  power_manager::PowerManagementPolicy expected_policy;
  expected_policy.mutable_ac_delays()->set_screen_dim_ms(
      prefs->GetInteger(prefs::kPowerAcScreenDimDelayMs));
  expected_policy.mutable_ac_delays()->set_screen_off_ms(
      prefs->GetInteger(prefs::kPowerAcScreenOffDelayMs));
  expected_policy.mutable_ac_delays()->set_screen_lock_ms(
      prefs->GetInteger(prefs::kPowerAcScreenLockDelayMs));
  expected_policy.mutable_ac_delays()->set_idle_warning_ms(
      prefs->GetInteger(prefs::kPowerAcIdleWarningDelayMs));
  expected_policy.mutable_ac_delays()->set_idle_ms(
      prefs->GetInteger(prefs::kPowerAcIdleDelayMs));
  expected_policy.mutable_battery_delays()->set_screen_dim_ms(
      prefs->GetInteger(prefs::kPowerBatteryScreenDimDelayMs));
  expected_policy.mutable_battery_delays()->set_screen_off_ms(
      prefs->GetInteger(prefs::kPowerBatteryScreenOffDelayMs));
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
  scoped_ptr<TestingPrefServiceSyncable> login_profile_prefs(
      new TestingPrefServiceSyncable);
  chrome::RegisterLoginProfilePrefs(login_profile_prefs->registry());
  TestingProfile::Builder builder;
  builder.SetPath(
      profile_manager_.profiles_dir().AppendASCII(chrome::kInitialProfile));
  builder.SetPrefService(login_profile_prefs.PassAs<PrefServiceSyncable>());
  builder.SetIncognito();
  scoped_ptr<TestingProfile> login_profile_owner(builder.Build());

  TestingProfile* login_profile = login_profile_owner.get();
  TestingProfile* login_profile_parent = profile_manager_.CreateTestingProfile(
      chrome::kInitialProfile);
  login_profile_parent->SetOffTheRecordProfile(
      login_profile_owner.PassAs<Profile>());
  login_profile->SetOriginalProfile(login_profile_parent);

  // Inform power_prefs_ that the login screen is being shown.
  power_prefs_->Observe(chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
                        content::Source<PowerPrefsTest>(this),
                        content::NotificationService::NoDetails());

  EXPECT_EQ(login_profile, GetProfile());
  EXPECT_EQ(GetExpectedPowerPolicyForProfile(login_profile),
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
  EXPECT_EQ(GetExpectedPowerPolicyForProfile(login_profile),
            GetCurrentPowerPolicy());
  EXPECT_EQ(GetExpectedAllowScreenWakeLocksForProfile(login_profile),
            GetCurrentAllowScreenWakeLocks());

  // Inform power_prefs_ that the login profile has been destroyed.
  power_prefs_->Observe(chrome::NOTIFICATION_PROFILE_DESTROYED,
                        content::Source<Profile>(login_profile),
                        content::NotificationService::NoDetails());

  // The login profile's prefs should still be used.
  EXPECT_FALSE(GetProfile());
  EXPECT_EQ(GetExpectedPowerPolicyForProfile(login_profile),
            GetCurrentPowerPolicy());
}

TEST_F(PowerPrefsTest, UserSession) {
  // Set up user profile.
  TestingProfile* user_profile = profile_manager_.CreateTestingProfile("user");
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(switches::kLoginProfile,
                                                      "user");
  profile_manager_.SetLoggedIn(true);

  // Inform power_prefs_ that a session has started.
  power_prefs_->Observe(chrome::NOTIFICATION_SESSION_STARTED,
                        content::Source<PowerPrefsTest>(this),
                        content::NotificationService::NoDetails());

  EXPECT_EQ(user_profile, GetProfile());
  EXPECT_EQ(GetExpectedPowerPolicyForProfile(user_profile),
            GetCurrentPowerPolicy());
  EXPECT_EQ(GetExpectedAllowScreenWakeLocksForProfile(user_profile),
            GetCurrentAllowScreenWakeLocks());

  TestingProfile* other_profile =
      profile_manager_.CreateTestingProfile("other");

  // Inform power_prefs_ that an unrelated profile has been destroyed.
  power_prefs_->Observe(chrome::NOTIFICATION_PROFILE_DESTROYED,
                        content::Source<Profile>(other_profile),
                        content::NotificationService::NoDetails());

  // Verify that the user profile's power prefs are still being used.
  EXPECT_EQ(user_profile, GetProfile());
  EXPECT_EQ(GetExpectedPowerPolicyForProfile(user_profile),
            GetCurrentPowerPolicy());
  EXPECT_EQ(GetExpectedAllowScreenWakeLocksForProfile(user_profile),
            GetCurrentAllowScreenWakeLocks());

  // Simulate the login screen coming up as part of screen locking.
  power_prefs_->Observe(chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
                        content::Source<PowerPrefsTest>(this),
                        content::NotificationService::NoDetails());

  // Verify that power policy didn't revert to login screen settings.
  EXPECT_EQ(user_profile, GetProfile());
  EXPECT_EQ(GetExpectedPowerPolicyForProfile(user_profile),
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
  EXPECT_EQ(GetExpectedPowerPolicyForProfile(user_profile),
            GetCurrentPowerPolicy());
}

}  // namespace chromeos
