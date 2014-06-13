// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/magnifier/magnification_controller.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/accessibility/magnification_manager.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/chromeos/login/users/user_manager_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/chromeos_switches.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

const char kTestUserName[] = "owner@invalid.domain";

void SetMagnifierEnabled(bool enabled) {
  MagnificationManager::Get()->SetMagnifierEnabled(enabled);
}

void SetMagnifierType(ash::MagnifierType type) {
  MagnificationManager::Get()->SetMagnifierType(type);
}

void SetFullScreenMagnifierScale(double scale) {
  ash::Shell::GetInstance()->
      magnification_controller()->SetScale(scale, false);
}

double GetFullScreenMagnifierScale() {
  return ash::Shell::GetInstance()->magnification_controller()->GetScale();
}

void SetSavedFullScreenMagnifierScale(double scale) {
  MagnificationManager::Get()->SaveScreenMagnifierScale(scale);
}

double GetSavedFullScreenMagnifierScale() {
  return MagnificationManager::Get()->GetSavedScreenMagnifierScale();
}

ash::MagnifierType GetMagnifierType() {
  return MagnificationManager::Get()->GetMagnifierType();
}

bool IsMagnifierEnabled() {
  return MagnificationManager::Get()->IsMagnifierEnabled();
}

Profile* profile() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  DCHECK(profile);
  return profile;
}

PrefService* prefs() {
  return user_prefs::UserPrefs::Get(profile());
}

void SetScreenMagnifierEnabledPref(bool enabled) {
  prefs()->SetBoolean(prefs::kAccessibilityScreenMagnifierEnabled, enabled);
}

void SetScreenMagnifierTypePref(ash::MagnifierType type) {
  prefs()->SetInteger(prefs::kAccessibilityScreenMagnifierType, type);
}

void SetFullScreenMagnifierScalePref(double scale) {
  prefs()->SetDouble(prefs::kAccessibilityScreenMagnifierScale, scale);
}

bool GetScreenMagnifierEnabledFromPref() {
  return prefs()->GetBoolean(prefs::kAccessibilityScreenMagnifierEnabled);
}

// Creates and logs into a profile with account |name|, and makes sure that
// the profile is regarded as "non new" in the next login. This is used in
// PRE_XXX cases so that in the main XXX case we can test non new profiles.
void PrepareNonNewProfile(const std::string& name) {
  UserManager::Get()->UserLoggedIn(name, name, true);
  // To prepare a non-new profile for tests, we must ensure the profile
  // directory and the preference files are created, because that's what
  // Profile::IsNewProfile() checks. UserLoggedIn(), however, does not yet
  // create the profile directory until GetActiveUserProfile() is called.
  ProfileManager::GetActiveUserProfile();
}

}  // namespace

class MockMagnificationObserver {
 public:
  MockMagnificationObserver() : observed_(false),
                                observed_enabled_(false),
                                magnifier_type_(-1)
  {
    AccessibilityManager* accessibility_manager = AccessibilityManager::Get();
    CHECK(accessibility_manager);
    accessibility_subscription_ = accessibility_manager->RegisterCallback(
        base::Bind(&MockMagnificationObserver::OnAccessibilityStatusChanged,
                   base::Unretained(this)));
  }

  virtual ~MockMagnificationObserver() {}

  bool observed() const { return observed_; }
  bool observed_enabled() const { return observed_enabled_; }
  int magnifier_type() const { return magnifier_type_; }

  void reset() { observed_ = false; }

 private:
  void OnAccessibilityStatusChanged(
      const AccessibilityStatusEventDetails& details) {
    if (details.notification_type == ACCESSIBILITY_TOGGLE_SCREEN_MAGNIFIER) {
      magnifier_type_ = details.magnifier_type;
      observed_enabled_ = details.enabled;
      observed_ = true;
    }
  }

  bool observed_;
  bool observed_enabled_;
  int magnifier_type_;

  scoped_ptr<AccessibilityStatusSubscription> accessibility_subscription_;

  DISALLOW_COPY_AND_ASSIGN(MockMagnificationObserver);
};


class MagnificationManagerTest : public InProcessBrowserTest {
 protected:
  MagnificationManagerTest() {}
  virtual ~MagnificationManagerTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kLoginManager);
    command_line->AppendSwitchASCII(switches::kLoginProfile,
                                    TestingProfile::kTestUserProfileDir);
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    // Set the login-screen profile.
    MagnificationManager::Get()->SetProfileForTest(
        ProfileManager::GetActiveUserProfile());
  }

  DISALLOW_COPY_AND_ASSIGN(MagnificationManagerTest);
};

IN_PROC_BROWSER_TEST_F(MagnificationManagerTest, PRE_LoginOffToOff) {
  // Create a new profile once, to run the test with non-new profile.
  PrepareNonNewProfile(kTestUserName);

  // Sets pref to explicitly disable the magnifier.
  SetScreenMagnifierEnabledPref(false);
}

IN_PROC_BROWSER_TEST_F(MagnificationManagerTest, LoginOffToOff) {
  // Confirms that magnifier is disabled on the login screen.
  EXPECT_FALSE(IsMagnifierEnabled());

  // Disables magnifier on login screen.
  SetMagnifierEnabled(false);
  EXPECT_FALSE(IsMagnifierEnabled());

  // Logs in with existing profile.
  UserManager::Get()->UserLoggedIn(kTestUserName, kTestUserName, true);

  // Confirms that magnifier is still disabled just after login.
  EXPECT_FALSE(IsMagnifierEnabled());

  UserManager::Get()->SessionStarted();

  // Confirms that magnifier is still disabled just after session starts.
  EXPECT_FALSE(IsMagnifierEnabled());

  // Enables magnifier.
  SetMagnifierEnabled(true);
  // Confirms that magnifier is enabled.
  EXPECT_TRUE(IsMagnifierEnabled());
  EXPECT_EQ(ash::MAGNIFIER_FULL, GetMagnifierType());
  EXPECT_TRUE(GetScreenMagnifierEnabledFromPref());
}

IN_PROC_BROWSER_TEST_F(MagnificationManagerTest, PRE_LoginFullToOff) {
  // Create a new profile once, to run the test with non-new profile.
  PrepareNonNewProfile(kTestUserName);

  // Sets pref to explicitly disable the magnifier.
  SetScreenMagnifierEnabledPref(false);
}

IN_PROC_BROWSER_TEST_F(MagnificationManagerTest, LoginFullToOff) {
  // Confirms that magnifier is disabled on the login screen.
  EXPECT_FALSE(IsMagnifierEnabled());

  // Enables magnifier on login screen.
  SetMagnifierEnabled(true);
  SetMagnifierType(ash::MAGNIFIER_FULL);
  SetFullScreenMagnifierScale(2.5);
  EXPECT_TRUE(IsMagnifierEnabled());
  EXPECT_EQ(ash::MAGNIFIER_FULL, GetMagnifierType());
  EXPECT_EQ(2.5, GetFullScreenMagnifierScale());

  // Logs in (but the session is not started yet).
  UserManager::Get()->UserLoggedIn(kTestUserName, kTestUserName, true);

  // Confirms that magnifier is keeping enabled.
  EXPECT_TRUE(IsMagnifierEnabled());
  EXPECT_EQ(ash::MAGNIFIER_FULL, GetMagnifierType());

  UserManager::Get()->SessionStarted();

  // Confirms that magnifier is disabled just after session start.
  EXPECT_FALSE(IsMagnifierEnabled());
  EXPECT_FALSE(GetScreenMagnifierEnabledFromPref());
}

IN_PROC_BROWSER_TEST_F(MagnificationManagerTest, PRE_LoginOffToFull) {
  // Create a new profile once, to run the test with non-new profile.
  PrepareNonNewProfile(kTestUserName);

  // Sets prefs to explicitly enable the magnifier.
  SetScreenMagnifierEnabledPref(true);
  SetScreenMagnifierTypePref(ash::MAGNIFIER_FULL);
  SetFullScreenMagnifierScalePref(2.5);
}

IN_PROC_BROWSER_TEST_F(MagnificationManagerTest, LoginOffToFull) {
  // Disables magnifier on login screen.
  SetMagnifierEnabled(false);
  EXPECT_FALSE(IsMagnifierEnabled());

  // Logs in (but the session is not started yet).
  UserManager::Get()->UserLoggedIn(kTestUserName, kTestUserName, true);

  // Confirms that magnifier is keeping disabled.
  EXPECT_FALSE(IsMagnifierEnabled());

  UserManager::Get()->SessionStarted();

  // Confirms that the magnifier is enabled and configured according to the
  // explicitly set prefs just after session start.
  EXPECT_TRUE(IsMagnifierEnabled());
  EXPECT_EQ(ash::MAGNIFIER_FULL, GetMagnifierType());
  EXPECT_EQ(2.5, GetFullScreenMagnifierScale());
  EXPECT_TRUE(GetScreenMagnifierEnabledFromPref());
}

IN_PROC_BROWSER_TEST_F(MagnificationManagerTest, PRE_LoginFullToFull) {
  // Create a new profile once, to run the test with non-new profile.
  PrepareNonNewProfile(kTestUserName);

  // Sets prefs to explicitly enable the magnifier.
  SetScreenMagnifierEnabledPref(true);
  SetScreenMagnifierTypePref(ash::MAGNIFIER_FULL);
  SetFullScreenMagnifierScalePref(2.5);
}

IN_PROC_BROWSER_TEST_F(MagnificationManagerTest, LoginFullToFull) {
  // Enables magnifier on login screen.
  SetMagnifierType(ash::MAGNIFIER_FULL);
  SetMagnifierEnabled(true);
  SetFullScreenMagnifierScale(3.0);
  EXPECT_TRUE(IsMagnifierEnabled());
  EXPECT_EQ(ash::MAGNIFIER_FULL, GetMagnifierType());
  EXPECT_EQ(3.0, GetFullScreenMagnifierScale());

  // Logs in (but the session is not started yet).
  UserManager::Get()->UserLoggedIn(kTestUserName, kTestUserName, true);

  // Confirms that magnifier is keeping enabled.
  EXPECT_TRUE(IsMagnifierEnabled());
  EXPECT_EQ(ash::MAGNIFIER_FULL, GetMagnifierType());

  UserManager::Get()->SessionStarted();

  // Confirms that the magnifier is enabled and configured according to the
  // explicitly set prefs just after session start.
  EXPECT_TRUE(IsMagnifierEnabled());
  EXPECT_EQ(ash::MAGNIFIER_FULL, GetMagnifierType());
  EXPECT_EQ(2.5, GetFullScreenMagnifierScale());
  EXPECT_TRUE(GetScreenMagnifierEnabledFromPref());
}

IN_PROC_BROWSER_TEST_F(MagnificationManagerTest, PRE_LoginFullToUnset) {
  // Creates a new profile once, to run the test with non-new profile.
  PrepareNonNewProfile(kTestUserName);
}

IN_PROC_BROWSER_TEST_F(MagnificationManagerTest, LoginFullToUnset) {
  // Enables full screen magnifier.
  SetMagnifierType(ash::MAGNIFIER_FULL);
  SetMagnifierEnabled(true);
  EXPECT_TRUE(IsMagnifierEnabled());
  EXPECT_EQ(ash::MAGNIFIER_FULL, GetMagnifierType());

  // Logs in (but the session is not started yet).
  UserManager::Get()->UserLoggedIn(kTestUserName, kTestUserName, true);

  // Confirms that magnifier is keeping enabled.
  EXPECT_TRUE(IsMagnifierEnabled());
  EXPECT_EQ(ash::MAGNIFIER_FULL, GetMagnifierType());

  UserManager::Get()->SessionStarted();

  // Confirms that magnifier is disabled.
  EXPECT_FALSE(IsMagnifierEnabled());
  EXPECT_FALSE(GetScreenMagnifierEnabledFromPref());
}

IN_PROC_BROWSER_TEST_F(MagnificationManagerTest, LoginAsNewUserOff) {
  // Confirms that magnifier is disabled on the login screen.
  EXPECT_FALSE(IsMagnifierEnabled());

  // Disables magnifier on login screen explicitly.
  SetMagnifierEnabled(false);

  // Logs in (but the session is not started yet).
  UserManager::Get()->UserLoggedIn(kTestUserName, kTestUserName, true);

  // Confirms that magnifier is keeping disabled.
  EXPECT_FALSE(IsMagnifierEnabled());

  UserManager::Get()->SessionStarted();

  // Confirms that magnifier is keeping disabled.
  EXPECT_FALSE(IsMagnifierEnabled());
  EXPECT_FALSE(GetScreenMagnifierEnabledFromPref());
}

IN_PROC_BROWSER_TEST_F(MagnificationManagerTest, LoginAsNewUserFull) {
  // Enables magnifier on login screen.
  SetMagnifierType(ash::MAGNIFIER_FULL);
  SetMagnifierEnabled(true);
  SetFullScreenMagnifierScale(2.5);
  EXPECT_TRUE(IsMagnifierEnabled());
  EXPECT_EQ(ash::MAGNIFIER_FULL, GetMagnifierType());
  EXPECT_EQ(2.5, GetFullScreenMagnifierScale());

  // Logs in (but the session is not started yet).
  UserManager::Get()->UserLoggedIn(kTestUserName, kTestUserName, true);

  // Confirms that magnifier is keeping enabled.
  EXPECT_TRUE(IsMagnifierEnabled());
  EXPECT_EQ(ash::MAGNIFIER_FULL, GetMagnifierType());

  UserManager::Get()->SessionStarted();

  // Confirms that magnifier keeps enabled.
  EXPECT_TRUE(IsMagnifierEnabled());
  EXPECT_EQ(ash::MAGNIFIER_FULL, GetMagnifierType());
  EXPECT_EQ(2.5, GetFullScreenMagnifierScale());
  EXPECT_TRUE(GetScreenMagnifierEnabledFromPref());
}

IN_PROC_BROWSER_TEST_F(MagnificationManagerTest, LoginAsNewUserUnset) {
  // Confirms that magnifier is disabled on the login screen.
  EXPECT_FALSE(IsMagnifierEnabled());

  // Logs in (but the session is not started yet).
  UserManager::Get()->UserLoggedIn(kTestUserName, kTestUserName, true);

  // Confirms that magnifier is keeping disabled.
  EXPECT_FALSE(IsMagnifierEnabled());

  UserManager::Get()->SessionStarted();

  // Confirms that magnifier is keeping disabled.
  EXPECT_FALSE(IsMagnifierEnabled());
  EXPECT_FALSE(GetScreenMagnifierEnabledFromPref());
}

IN_PROC_BROWSER_TEST_F(MagnificationManagerTest, ChangeMagnifierType) {
  // Enables/disables full screen magnifier.
  SetMagnifierEnabled(false);
  SetMagnifierType(ash::MAGNIFIER_FULL);
  EXPECT_FALSE(IsMagnifierEnabled());
  EXPECT_EQ(ash::MAGNIFIER_FULL, GetMagnifierType());

  SetMagnifierEnabled(true);
  EXPECT_TRUE(IsMagnifierEnabled());
  EXPECT_EQ(ash::MAGNIFIER_FULL, GetMagnifierType());

  SetMagnifierEnabled(false);
  EXPECT_FALSE(IsMagnifierEnabled());
  EXPECT_EQ(ash::MAGNIFIER_FULL, GetMagnifierType());

  // Enables/disables partial screen magnifier.
  SetMagnifierType(ash::MAGNIFIER_PARTIAL);
  EXPECT_FALSE(IsMagnifierEnabled());
  EXPECT_EQ(ash::MAGNIFIER_FULL, GetMagnifierType());

  SetMagnifierEnabled(true);
  EXPECT_TRUE(IsMagnifierEnabled());
  EXPECT_EQ(ash::MAGNIFIER_FULL, GetMagnifierType());

  SetMagnifierEnabled(false);
  EXPECT_FALSE(IsMagnifierEnabled());
  EXPECT_EQ(ash::MAGNIFIER_FULL, GetMagnifierType());

  // Changes the magnifier type when the magnifier is enabled.
  SetMagnifierType(ash::MAGNIFIER_FULL);
  SetMagnifierEnabled(true);
  EXPECT_TRUE(IsMagnifierEnabled());
  EXPECT_EQ(ash::MAGNIFIER_FULL, GetMagnifierType());

  SetMagnifierType(ash::MAGNIFIER_PARTIAL);
  EXPECT_TRUE(IsMagnifierEnabled());
  EXPECT_EQ(ash::MAGNIFIER_FULL, GetMagnifierType());

  SetMagnifierType(ash::MAGNIFIER_FULL);
  EXPECT_TRUE(IsMagnifierEnabled());
  EXPECT_EQ(ash::MAGNIFIER_FULL, GetMagnifierType());

  // Changes the magnifier type when the magnifier is disabled.
  SetMagnifierEnabled(false);
  SetMagnifierType(ash::MAGNIFIER_FULL);
  EXPECT_FALSE(IsMagnifierEnabled());
  EXPECT_EQ(ash::MAGNIFIER_FULL, GetMagnifierType());

  SetMagnifierType(ash::MAGNIFIER_PARTIAL);
  EXPECT_FALSE(IsMagnifierEnabled());
  EXPECT_EQ(ash::MAGNIFIER_FULL, GetMagnifierType());

  SetMagnifierType(ash::MAGNIFIER_FULL);
  EXPECT_FALSE(IsMagnifierEnabled());
  EXPECT_EQ(ash::MAGNIFIER_FULL, GetMagnifierType());
}

IN_PROC_BROWSER_TEST_F(MagnificationManagerTest, TypePref) {
  // Logs in
  UserManager::Get()->UserLoggedIn(kTestUserName, kTestUserName, true);
  UserManager::Get()->SessionStarted();

  // Confirms that magnifier is disabled just after login.
  EXPECT_FALSE(IsMagnifierEnabled());

  // Sets the pref as true to enable magnifier.
  SetScreenMagnifierTypePref(ash::MAGNIFIER_FULL);
  SetScreenMagnifierEnabledPref(true);
  // Confirms that magnifier is enabled.
  EXPECT_TRUE(IsMagnifierEnabled());
  EXPECT_EQ(ash::MAGNIFIER_FULL, GetMagnifierType());
}

IN_PROC_BROWSER_TEST_F(MagnificationManagerTest, ScalePref) {
  SetMagnifierEnabled(false);
  EXPECT_FALSE(IsMagnifierEnabled());

  // Sets 2.5x to the pref.
  SetSavedFullScreenMagnifierScale(2.5);

  // Enables full screen magnifier.
  SetMagnifierType(ash::MAGNIFIER_FULL);
  SetMagnifierEnabled(true);
  EXPECT_TRUE(IsMagnifierEnabled());
  EXPECT_EQ(ash::MAGNIFIER_FULL, GetMagnifierType());

  // Confirms that 2.5x is restored.
  EXPECT_EQ(2.5, GetFullScreenMagnifierScale());

  // Sets the scale and confirms that the scale is saved to pref.
  SetFullScreenMagnifierScale(3.0);
  EXPECT_EQ(3.0, GetSavedFullScreenMagnifierScale());
}

IN_PROC_BROWSER_TEST_F(MagnificationManagerTest, InvalidScalePref) {
  // TEST 1: Sets too small scale
  SetMagnifierEnabled(false);
  EXPECT_FALSE(IsMagnifierEnabled());

  // Sets too small value to the pref.
  SetSavedFullScreenMagnifierScale(0.5);

  // Enables full screen magnifier.
  SetMagnifierType(ash::MAGNIFIER_FULL);
  SetMagnifierEnabled(true);
  EXPECT_TRUE(IsMagnifierEnabled());
  EXPECT_EQ(ash::MAGNIFIER_FULL, GetMagnifierType());

  // Confirms that the actual scale is set to the minimum scale.
  EXPECT_EQ(1.0, GetFullScreenMagnifierScale());

  // TEST 2: Sets too large scale
  SetMagnifierEnabled(false);
  EXPECT_FALSE(IsMagnifierEnabled());

  // Sets too large value to the pref.
  SetSavedFullScreenMagnifierScale(50.0);

  // Enables full screen magnifier.
  SetMagnifierEnabled(true);
  EXPECT_TRUE(IsMagnifierEnabled());
  EXPECT_EQ(ash::MAGNIFIER_FULL, GetMagnifierType());

  // Confirms that the actual scale is set to the maximum scale.
  EXPECT_EQ(4.0, GetFullScreenMagnifierScale());
}

IN_PROC_BROWSER_TEST_F(MagnificationManagerTest,
                       ChangingTypeInvokesNotification) {
  MockMagnificationObserver observer;

  EXPECT_FALSE(observer.observed());

  // Set full screen magnifier, and confirm the observer is called.
  SetMagnifierEnabled(true);
  SetMagnifierType(ash::MAGNIFIER_FULL);
  EXPECT_TRUE(observer.observed());
  EXPECT_TRUE(observer.observed_enabled());
  EXPECT_EQ(observer.magnifier_type(), ash::MAGNIFIER_FULL);
  EXPECT_EQ(GetMagnifierType(), ash::MAGNIFIER_FULL);
  observer.reset();

  // Set full screen magnifier again, and confirm the observer is not called.
  SetMagnifierType(ash::MAGNIFIER_FULL);
  EXPECT_FALSE(observer.observed());
  EXPECT_EQ(GetMagnifierType(), ash::MAGNIFIER_FULL);
  observer.reset();
}

}  // namespace chromeos
