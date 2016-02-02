// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/recommendation_restorer.h"

#include <utility>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_simple_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/policy/recommendation_restorer_factory.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_notifier_impl.h"
#include "components/prefs/testing_pref_store.h"
#include "components/syncable_prefs/pref_service_syncable.h"
#include "components/syncable_prefs/testing_pref_service_syncable.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/chromeos/accessibility_types.h"

namespace policy {

namespace {
  // The amount of idle time after which recommended values are restored.
  const int kRestoreDelayInMs = 60 * 1000;  // 1 minute.
}  // namespace

class RecommendationRestorerTest : public testing::Test {
 protected:
  RecommendationRestorerTest();

  // testing::Test:
  void SetUp() override;

  void RegisterUserProfilePrefs();
  void RegisterLoginProfilePrefs();

  void SetRecommendedValues();
  void SetUserSettings();

  void CreateLoginProfile();
  void CreateUserProfile();

  void NotifyOfSessionStart();
  void NotifyOfUserActivity();

  void VerifyPrefFollowsUser(const char* pref_name,
                             const base::Value& expected_value) const;
  void VerifyPrefsFollowUser() const;
  void VerifyPrefFollowsRecommendation(const char* pref_name,
                                       const base::Value& expected_value) const;
  void VerifyPrefsFollowRecommendations() const;

  void VerifyNotListeningForNotifications() const;
  void VerifyTimerIsStopped() const;
  void VerifyTimerIsRunning() const;

  TestingPrefStore* recommended_prefs_;  // Not owned.
  syncable_prefs::TestingPrefServiceSyncable* prefs_;  // Not owned.
  RecommendationRestorer* restorer_;     // Not owned.

  scoped_refptr<base::TestSimpleTaskRunner> runner_;
  base::ThreadTaskRunnerHandle runner_handler_;

 private:
  scoped_ptr<syncable_prefs::PrefServiceSyncable> prefs_owner_;

  TestingProfileManager profile_manager_;

  DISALLOW_COPY_AND_ASSIGN(RecommendationRestorerTest);
};

RecommendationRestorerTest::RecommendationRestorerTest()
    : recommended_prefs_(new TestingPrefStore),
      prefs_(new syncable_prefs::TestingPrefServiceSyncable(
          new TestingPrefStore,
          new TestingPrefStore,
          recommended_prefs_,
          new user_prefs::PrefRegistrySyncable,
          new PrefNotifierImpl)),
      restorer_(NULL),
      runner_(new base::TestSimpleTaskRunner),
      runner_handler_(runner_),
      prefs_owner_(prefs_),
      profile_manager_(TestingBrowserProcess::GetGlobal()) {}

void RecommendationRestorerTest::SetUp() {
  testing::Test::SetUp();
  ASSERT_TRUE(profile_manager_.SetUp());
}

void RecommendationRestorerTest::RegisterUserProfilePrefs() {
  chrome::RegisterUserProfilePrefs(prefs_->registry());
}

void RecommendationRestorerTest::RegisterLoginProfilePrefs() {
  chrome::RegisterLoginProfilePrefs(prefs_->registry());
}

void RecommendationRestorerTest::SetRecommendedValues() {
  recommended_prefs_->SetBoolean(prefs::kAccessibilityLargeCursorEnabled,
                                 false);
  recommended_prefs_->SetBoolean(prefs::kAccessibilitySpokenFeedbackEnabled,
                                 false);
  recommended_prefs_->SetBoolean(prefs::kAccessibilityHighContrastEnabled,
                                 false);
  recommended_prefs_->SetBoolean(prefs::kAccessibilityScreenMagnifierEnabled,
                                 false);
  recommended_prefs_->SetInteger(prefs::kAccessibilityScreenMagnifierType, 0);
  recommended_prefs_->SetBoolean(prefs::kAccessibilityVirtualKeyboardEnabled,
                                 false);
}

void RecommendationRestorerTest::SetUserSettings() {
  prefs_->SetBoolean(prefs::kAccessibilityLargeCursorEnabled, true);
  prefs_->SetBoolean(prefs::kAccessibilitySpokenFeedbackEnabled, true);
  prefs_->SetBoolean(prefs::kAccessibilityHighContrastEnabled, true);
  prefs_->SetBoolean(prefs::kAccessibilityScreenMagnifierEnabled, true);
  prefs_->SetInteger(prefs::kAccessibilityScreenMagnifierType,
                     ui::MAGNIFIER_FULL);
  prefs_->SetBoolean(prefs::kAccessibilityVirtualKeyboardEnabled, true);
}

void RecommendationRestorerTest::CreateLoginProfile() {
  ASSERT_FALSE(restorer_);
  TestingProfile* profile = profile_manager_.CreateTestingProfile(
      chrome::kInitialProfile, std::move(prefs_owner_),
      base::UTF8ToUTF16(chrome::kInitialProfile), 0, std::string(),
      TestingProfile::TestingFactories());
  restorer_ = RecommendationRestorerFactory::GetForProfile(profile);
  EXPECT_TRUE(restorer_);
}

void RecommendationRestorerTest::CreateUserProfile() {
  ASSERT_FALSE(restorer_);
  TestingProfile* profile = profile_manager_.CreateTestingProfile(
      "user", std::move(prefs_owner_), base::UTF8ToUTF16("user"), 0,
      std::string(), TestingProfile::TestingFactories());
  restorer_ = RecommendationRestorerFactory::GetForProfile(profile);
  EXPECT_TRUE(restorer_);
}

void RecommendationRestorerTest::NotifyOfSessionStart() {
  ASSERT_TRUE(restorer_);
  restorer_->Observe(chrome::NOTIFICATION_LOGIN_USER_CHANGED,
                     content::Source<RecommendationRestorerTest>(this),
                     content::NotificationService::NoDetails());
}

void RecommendationRestorerTest::NotifyOfUserActivity() {
  ASSERT_TRUE(restorer_);
  restorer_->OnUserActivity(NULL);
}

void RecommendationRestorerTest::VerifyPrefFollowsUser(
    const char* pref_name,
    const base::Value& expected_value) const {
  const syncable_prefs::PrefServiceSyncable::Preference* pref =
      prefs_->FindPreference(pref_name);
  ASSERT_TRUE(pref);
  EXPECT_TRUE(pref->HasUserSetting());
  const base::Value* value = pref->GetValue();
  ASSERT_TRUE(value);
  EXPECT_TRUE(expected_value.Equals(value));
}

void RecommendationRestorerTest::VerifyPrefsFollowUser() const {
  VerifyPrefFollowsUser(prefs::kAccessibilityLargeCursorEnabled,
                        base::FundamentalValue(true));
  VerifyPrefFollowsUser(prefs::kAccessibilitySpokenFeedbackEnabled,
                        base::FundamentalValue(true));
  VerifyPrefFollowsUser(prefs::kAccessibilityHighContrastEnabled,
                        base::FundamentalValue(true));
  VerifyPrefFollowsUser(prefs::kAccessibilityScreenMagnifierEnabled,
                        base::FundamentalValue(true));
  VerifyPrefFollowsUser(prefs::kAccessibilityScreenMagnifierType,
                        base::FundamentalValue(ui::MAGNIFIER_FULL));
  VerifyPrefFollowsUser(prefs::kAccessibilityVirtualKeyboardEnabled,
                        base::FundamentalValue(true));
}

void RecommendationRestorerTest::VerifyPrefFollowsRecommendation(
    const char* pref_name,
    const base::Value& expected_value) const {
  const syncable_prefs::PrefServiceSyncable::Preference* pref =
      prefs_->FindPreference(pref_name);
  ASSERT_TRUE(pref);
  EXPECT_TRUE(pref->IsRecommended());
  EXPECT_FALSE(pref->HasUserSetting());
  const base::Value* value = pref->GetValue();
  ASSERT_TRUE(value);
  EXPECT_TRUE(expected_value.Equals(value));
}

void RecommendationRestorerTest::VerifyPrefsFollowRecommendations() const {
  VerifyPrefFollowsRecommendation(prefs::kAccessibilityLargeCursorEnabled,
                                  base::FundamentalValue(false));
  VerifyPrefFollowsRecommendation(prefs::kAccessibilitySpokenFeedbackEnabled,
                                  base::FundamentalValue(false));
  VerifyPrefFollowsRecommendation(prefs::kAccessibilityHighContrastEnabled,
                                  base::FundamentalValue(false));
  VerifyPrefFollowsRecommendation(prefs::kAccessibilityScreenMagnifierEnabled,
                                  base::FundamentalValue(false));
  VerifyPrefFollowsRecommendation(prefs::kAccessibilityScreenMagnifierType,
                                  base::FundamentalValue(0));
  VerifyPrefFollowsRecommendation(prefs::kAccessibilityVirtualKeyboardEnabled,
                                  base::FundamentalValue(false));
}

void RecommendationRestorerTest::VerifyNotListeningForNotifications() const {
  ASSERT_TRUE(restorer_);
  EXPECT_TRUE(restorer_->pref_change_registrar_.IsEmpty());
  EXPECT_TRUE(restorer_->notification_registrar_.IsEmpty());
}

void RecommendationRestorerTest::VerifyTimerIsStopped() const {
  ASSERT_TRUE(restorer_);
  EXPECT_FALSE(restorer_->restore_timer_.IsRunning());
}

void RecommendationRestorerTest::VerifyTimerIsRunning() const {
  ASSERT_TRUE(restorer_);
  EXPECT_TRUE(restorer_->restore_timer_.IsRunning());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(kRestoreDelayInMs),
            restorer_->restore_timer_.GetCurrentDelay());
}

TEST_F(RecommendationRestorerTest, CreateForUserProfile) {
  // Verifies that when a RecommendationRestorer is created for a user profile,
  // it does not start listening for any notifications, does not clear user
  // settings on initialization and does not start a timer that will clear user
  // settings eventually.
  RegisterUserProfilePrefs();
  SetRecommendedValues();
  SetUserSettings();

  CreateUserProfile();
  VerifyNotListeningForNotifications();
  VerifyPrefsFollowUser();
  VerifyTimerIsStopped();
}

TEST_F(RecommendationRestorerTest, NoRecommendations) {
  // Verifies that when no recommended values have been set and a
  // RecommendationRestorer is created for the login profile, it does not clear
  // user settings on initialization and does not start a timer that will clear
  // user settings eventually.
  RegisterLoginProfilePrefs();
  SetUserSettings();

  CreateLoginProfile();
  VerifyPrefsFollowUser();
  VerifyTimerIsStopped();
}

TEST_F(RecommendationRestorerTest, RestoreOnStartup) {
  // Verifies that when recommended values have been set and a
  // RecommendationRestorer is created for the login profile, it clears user
  // settings on initialization.
  RegisterLoginProfilePrefs();
  SetRecommendedValues();
  SetUserSettings();

  CreateLoginProfile();
  VerifyPrefsFollowRecommendations();
  VerifyTimerIsStopped();
}

TEST_F(RecommendationRestorerTest, RestoreOnRecommendationChangeOnLoginScreen) {
  // Verifies that if recommended values change while the login screen is being
  // shown, a timer is started that will clear user settings eventually.
  RegisterLoginProfilePrefs();
  SetUserSettings();

  CreateLoginProfile();

  VerifyTimerIsStopped();
  recommended_prefs_->SetBoolean(prefs::kAccessibilityLargeCursorEnabled,
                                 false);
  VerifyPrefFollowsUser(prefs::kAccessibilityLargeCursorEnabled,
                        base::FundamentalValue(true));
  VerifyTimerIsRunning();
  runner_->RunUntilIdle();
  VerifyPrefFollowsRecommendation(prefs::kAccessibilityLargeCursorEnabled,
                                  base::FundamentalValue(false));

  VerifyTimerIsStopped();
  recommended_prefs_->SetBoolean(prefs::kAccessibilitySpokenFeedbackEnabled,
                                 false);
  VerifyPrefFollowsUser(prefs::kAccessibilitySpokenFeedbackEnabled,
                        base::FundamentalValue(true));
  VerifyTimerIsRunning();
  runner_->RunUntilIdle();
  VerifyPrefFollowsRecommendation(prefs::kAccessibilitySpokenFeedbackEnabled,
                                  base::FundamentalValue(false));

  VerifyTimerIsStopped();
  recommended_prefs_->SetBoolean(prefs::kAccessibilityHighContrastEnabled,
                                 false);
  VerifyPrefFollowsUser(prefs::kAccessibilityHighContrastEnabled,
                        base::FundamentalValue(true));
  VerifyTimerIsRunning();
  runner_->RunUntilIdle();
  VerifyPrefFollowsRecommendation(prefs::kAccessibilityHighContrastEnabled,
                                  base::FundamentalValue(false));

  VerifyTimerIsStopped();
  recommended_prefs_->SetBoolean(prefs::kAccessibilityScreenMagnifierEnabled,
                                 false);
  recommended_prefs_->SetInteger(prefs::kAccessibilityScreenMagnifierType, 0);
  VerifyPrefFollowsUser(prefs::kAccessibilityScreenMagnifierEnabled,
                        base::FundamentalValue(true));
  VerifyPrefFollowsUser(prefs::kAccessibilityScreenMagnifierType,
                        base::FundamentalValue(ui::MAGNIFIER_FULL));
  VerifyTimerIsRunning();
  runner_->RunUntilIdle();
  VerifyPrefFollowsRecommendation(prefs::kAccessibilityScreenMagnifierEnabled,
                                  base::FundamentalValue(false));
  VerifyPrefFollowsRecommendation(prefs::kAccessibilityScreenMagnifierType,
                                  base::FundamentalValue(0));
  VerifyTimerIsStopped();
  recommended_prefs_->SetBoolean(prefs::kAccessibilityVirtualKeyboardEnabled,
                                 false);
  VerifyPrefFollowsUser(prefs::kAccessibilityVirtualKeyboardEnabled,
                        base::FundamentalValue(true));
  VerifyTimerIsRunning();
  runner_->RunUntilIdle();
  VerifyPrefFollowsRecommendation(prefs::kAccessibilityVirtualKeyboardEnabled,
                                  base::FundamentalValue(false));
  VerifyTimerIsStopped();
}

TEST_F(RecommendationRestorerTest, RestoreOnRecommendationChangeInUserSession) {
  // Verifies that if recommended values change while a user session is in
  // progress, user settings are cleared immediately.
  RegisterLoginProfilePrefs();
  SetUserSettings();

  CreateLoginProfile();
  NotifyOfSessionStart();

  VerifyPrefFollowsUser(prefs::kAccessibilityLargeCursorEnabled,
                        base::FundamentalValue(true));
  recommended_prefs_->SetBoolean(prefs::kAccessibilityLargeCursorEnabled,
                                 false);
  VerifyTimerIsStopped();
  VerifyPrefFollowsRecommendation(prefs::kAccessibilityLargeCursorEnabled,
                                  base::FundamentalValue(false));

  VerifyPrefFollowsUser(prefs::kAccessibilitySpokenFeedbackEnabled,
                        base::FundamentalValue(true));
  recommended_prefs_->SetBoolean(prefs::kAccessibilitySpokenFeedbackEnabled,
                                 false);
  VerifyTimerIsStopped();
  VerifyPrefFollowsRecommendation(prefs::kAccessibilitySpokenFeedbackEnabled,
                                  base::FundamentalValue(false));

  VerifyPrefFollowsUser(prefs::kAccessibilityHighContrastEnabled,
                        base::FundamentalValue(true));
  recommended_prefs_->SetBoolean(prefs::kAccessibilityHighContrastEnabled,
                                 false);
  VerifyTimerIsStopped();
  VerifyPrefFollowsRecommendation(prefs::kAccessibilityHighContrastEnabled,
                                  base::FundamentalValue(false));

  VerifyPrefFollowsUser(prefs::kAccessibilityScreenMagnifierEnabled,
                        base::FundamentalValue(true));
  VerifyPrefFollowsUser(prefs::kAccessibilityScreenMagnifierType,
                        base::FundamentalValue(ui::MAGNIFIER_FULL));
  recommended_prefs_->SetBoolean(prefs::kAccessibilityScreenMagnifierEnabled,
                                 false);
  recommended_prefs_->SetInteger(prefs::kAccessibilityScreenMagnifierType, 0);
  VerifyTimerIsStopped();
  VerifyPrefFollowsRecommendation(prefs::kAccessibilityScreenMagnifierEnabled,
                                  base::FundamentalValue(false));
  VerifyPrefFollowsRecommendation(prefs::kAccessibilityScreenMagnifierType,
                                  base::FundamentalValue(0));

  VerifyPrefFollowsUser(prefs::kAccessibilityVirtualKeyboardEnabled,
                        base::FundamentalValue(true));
  recommended_prefs_->SetBoolean(prefs::kAccessibilityVirtualKeyboardEnabled,
                                 false);
  VerifyTimerIsStopped();
  VerifyPrefFollowsRecommendation(prefs::kAccessibilityVirtualKeyboardEnabled,
                                  base::FundamentalValue(false));
}

TEST_F(RecommendationRestorerTest, DoNothingOnUserChange) {
  // Verifies that if no recommended values have been set and user settings
  // change, the user settings are not cleared immediately and no timer is
  // started that will clear the user settings eventually.
  RegisterLoginProfilePrefs();
  CreateLoginProfile();

  prefs_->SetBoolean(prefs::kAccessibilityLargeCursorEnabled, true);
  VerifyPrefFollowsUser(prefs::kAccessibilityLargeCursorEnabled,
                        base::FundamentalValue(true));
  VerifyTimerIsStopped();

  prefs_->SetBoolean(prefs::kAccessibilitySpokenFeedbackEnabled, true);
  VerifyPrefFollowsUser(prefs::kAccessibilitySpokenFeedbackEnabled,
                        base::FundamentalValue(true));
  VerifyTimerIsStopped();

  prefs_->SetBoolean(prefs::kAccessibilityHighContrastEnabled, true);
  VerifyPrefFollowsUser(prefs::kAccessibilityHighContrastEnabled,
                        base::FundamentalValue(true));
  VerifyTimerIsStopped();

  prefs_->SetBoolean(prefs::kAccessibilityScreenMagnifierEnabled, true);
  VerifyPrefFollowsUser(prefs::kAccessibilityScreenMagnifierEnabled,
                        base::FundamentalValue(true));
  VerifyTimerIsStopped();

  prefs_->SetBoolean(prefs::kAccessibilityScreenMagnifierEnabled, true);
  prefs_->SetInteger(prefs::kAccessibilityScreenMagnifierType,
                     ui::MAGNIFIER_FULL);
  VerifyPrefFollowsUser(prefs::kAccessibilityScreenMagnifierEnabled,
                        base::FundamentalValue(true));
  VerifyPrefFollowsUser(prefs::kAccessibilityScreenMagnifierType,
                        base::FundamentalValue(ui::MAGNIFIER_FULL));
  VerifyTimerIsStopped();

  prefs_->SetBoolean(prefs::kAccessibilityVirtualKeyboardEnabled, true);
  VerifyPrefFollowsUser(prefs::kAccessibilityVirtualKeyboardEnabled,
                        base::FundamentalValue(true));
  VerifyTimerIsStopped();
}

TEST_F(RecommendationRestorerTest, RestoreOnUserChange) {
  // Verifies that if recommended values have been set and user settings change
  // while the login screen is being shown, a timer is started that will clear
  // the user settings eventually.
  RegisterLoginProfilePrefs();
  SetRecommendedValues();

  CreateLoginProfile();

  VerifyTimerIsStopped();
  prefs_->SetBoolean(prefs::kAccessibilityLargeCursorEnabled, true);
  VerifyPrefFollowsUser(prefs::kAccessibilityLargeCursorEnabled,
                        base::FundamentalValue(true));
  VerifyTimerIsRunning();
  runner_->RunUntilIdle();
  VerifyPrefFollowsRecommendation(prefs::kAccessibilityLargeCursorEnabled,
                                  base::FundamentalValue(false));

  VerifyTimerIsStopped();
  prefs_->SetBoolean(prefs::kAccessibilitySpokenFeedbackEnabled, true);
  VerifyPrefFollowsUser(prefs::kAccessibilitySpokenFeedbackEnabled,
                        base::FundamentalValue(true));
  VerifyTimerIsRunning();
  runner_->RunUntilIdle();
  VerifyPrefFollowsRecommendation(prefs::kAccessibilitySpokenFeedbackEnabled,
                                  base::FundamentalValue(false));

  VerifyTimerIsStopped();
  prefs_->SetBoolean(prefs::kAccessibilityHighContrastEnabled, true);
  VerifyPrefFollowsUser(prefs::kAccessibilityHighContrastEnabled,
                        base::FundamentalValue(true));
  VerifyTimerIsRunning();
  runner_->RunUntilIdle();
  VerifyPrefFollowsRecommendation(prefs::kAccessibilityHighContrastEnabled,
                                  base::FundamentalValue(false));

  VerifyTimerIsStopped();
  prefs_->SetBoolean(prefs::kAccessibilityScreenMagnifierEnabled, true);
  prefs_->SetInteger(prefs::kAccessibilityScreenMagnifierType,
                     ui::MAGNIFIER_FULL);
  VerifyPrefFollowsUser(prefs::kAccessibilityScreenMagnifierEnabled,
                        base::FundamentalValue(true));
  VerifyPrefFollowsUser(prefs::kAccessibilityScreenMagnifierType,
                        base::FundamentalValue(ui::MAGNIFIER_FULL));
  VerifyTimerIsRunning();
  runner_->RunUntilIdle();
  VerifyPrefFollowsRecommendation(prefs::kAccessibilityScreenMagnifierEnabled,
                                  base::FundamentalValue(false));
  VerifyPrefFollowsRecommendation(prefs::kAccessibilityScreenMagnifierType,
                                  base::FundamentalValue(0));

  VerifyTimerIsStopped();
  prefs_->SetBoolean(prefs::kAccessibilityVirtualKeyboardEnabled, true);
  VerifyPrefFollowsUser(prefs::kAccessibilityVirtualKeyboardEnabled,
                        base::FundamentalValue(true));
  VerifyTimerIsRunning();
  runner_->RunUntilIdle();
  VerifyPrefFollowsRecommendation(prefs::kAccessibilityVirtualKeyboardEnabled,
                                  base::FundamentalValue(false));

  VerifyTimerIsStopped();
}

TEST_F(RecommendationRestorerTest, RestoreOnSessionStart) {
  // Verifies that if recommended values have been set, user settings have
  // changed and a session is then started, the user settings are cleared
  // immediately and the timer that would have cleared them eventually on the
  // login screen is stopped.
  RegisterLoginProfilePrefs();
  SetRecommendedValues();

  CreateLoginProfile();
  SetUserSettings();

  NotifyOfSessionStart();
  VerifyPrefsFollowRecommendations();
  VerifyTimerIsStopped();
}

TEST_F(RecommendationRestorerTest, DoNothingOnSessionStart) {
  // Verifies that if recommended values have not been set, user settings have
  // changed and a session is then started, the user settings are not cleared
  // immediately.
  RegisterLoginProfilePrefs();
  CreateLoginProfile();
  SetUserSettings();

  NotifyOfSessionStart();
  VerifyPrefsFollowUser();
  VerifyTimerIsStopped();
}

TEST_F(RecommendationRestorerTest, UserActivityResetsTimer) {
  // Verifies that user activity resets the timer which clears user settings.
  RegisterLoginProfilePrefs();

  recommended_prefs_->SetBoolean(prefs::kAccessibilityLargeCursorEnabled,
                                 false);

  CreateLoginProfile();

  prefs_->SetBoolean(prefs::kAccessibilityLargeCursorEnabled, true);
  VerifyTimerIsRunning();

  // Notify that there is user activity, then fast forward until the originally
  // set timer fires.
  NotifyOfUserActivity();
  runner_->RunPendingTasks();
  VerifyPrefFollowsUser(prefs::kAccessibilityLargeCursorEnabled,
                        base::FundamentalValue(true));

  // Fast forward until the reset timer fires.
  VerifyTimerIsRunning();
  runner_->RunUntilIdle();
  VerifyPrefFollowsRecommendation(prefs::kAccessibilityLargeCursorEnabled,
                                  base::FundamentalValue(false));
  VerifyTimerIsStopped();
}

}  // namespace policy
