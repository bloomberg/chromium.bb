// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"

#include "ash/magnifier/magnification_controller.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/accessibility/magnification_manager.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/user_manager_impl.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/chromeos_switches.h"
#include "content/public/browser/notification_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

const char kTestUserName[] = "owner@invalid.domain";

// Test user name for locally managed user. The domain part must be matched
// with UserManager::kLocallyManagedUserDomain.
const char kTestLocallyManagedUserName[] = "test@locally-managed.localhost";

class MockAccessibilityObserver : public content::NotificationObserver {
 public:
  MockAccessibilityObserver() : observed_(false),
                                observed_enabled_(false),
                                observed_type_(-1) {
    registrar_.Add(
        this,
        chrome::NOTIFICATION_CROS_ACCESSIBILITY_TOGGLE_SPOKEN_FEEDBACK,
        content::NotificationService::AllSources());
    registrar_.Add(
        this,
        chrome::NOTIFICATION_CROS_ACCESSIBILITY_TOGGLE_HIGH_CONTRAST_MODE,
        content::NotificationService::AllSources());
  }
  virtual ~MockAccessibilityObserver() {}

  bool observed() const { return observed_; }
  bool observed_enabled() const { return observed_enabled_; }
  int observed_type() const { return observed_type_; }

  void reset() { observed_ = false; }

 private:
  // content::NotificationObserver implimentation:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    AccessibilityStatusEventDetails* accessibility_status =
        content::Details<AccessibilityStatusEventDetails>(
            details).ptr();
    ASSERT_FALSE(observed_);

    switch (type) {
      case chrome::NOTIFICATION_CROS_ACCESSIBILITY_TOGGLE_SPOKEN_FEEDBACK:
        observed_ = true;
        observed_enabled_ = accessibility_status->enabled;
        observed_type_ = type;
        break;
      case chrome::NOTIFICATION_CROS_ACCESSIBILITY_TOGGLE_HIGH_CONTRAST_MODE:
        observed_ = true;
        observed_enabled_ = accessibility_status->enabled;
        observed_type_ = type;
        break;
    }
  }

  bool observed_;
  bool observed_enabled_;
  int observed_type_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(MockAccessibilityObserver);
};

void SetLargeCursorEnabled(bool enabled) {
  return AccessibilityManager::Get()->EnableLargeCursor(enabled);
}

bool IsLargeCursorEnabled() {
  return AccessibilityManager::Get()->IsLargeCursorEnabled();
}

void SetHighContrastEnabled(bool enabled) {
  return AccessibilityManager::Get()->EnableHighContrast(enabled);
}

bool IsHighContrastEnabled() {
  return AccessibilityManager::Get()->IsHighContrastEnabled();
}

void SetSpokenFeedbackEnabled(bool enabled) {
  return AccessibilityManager::Get()->EnableSpokenFeedback(
      enabled, ash::A11Y_NOTIFICATION_NONE);
}

bool IsSpokenFeedbackEnabled() {
  return AccessibilityManager::Get()->IsSpokenFeedbackEnabled();
}

void SetAutoclickEnabled(bool enabled) {
  return AccessibilityManager::Get()->EnableAutoclick(enabled);
}

bool IsAutoclickEnabled() {
  return AccessibilityManager::Get()->IsAutoclickEnabled();
}

Profile* GetProfile() {
  Profile* profile = ProfileManager::GetDefaultProfileOrOffTheRecord();
  DCHECK(profile);
  return profile;
}

PrefService* GetPrefs() {
  return GetProfile()->GetPrefs();
}

void SetLargeCursorEnabledPref(bool enabled) {
  GetPrefs()->SetBoolean(prefs::kLargeCursorEnabled, enabled);
}

void SetHighContrastEnabledPref(bool enabled) {
  GetPrefs()->SetBoolean(prefs::kHighContrastEnabled, enabled);
}

void SetSpokenFeedbackEnabledPref(bool enabled) {
  GetPrefs()->SetBoolean(prefs::kSpokenFeedbackEnabled, enabled);
}

void SetAutoclickEnabledPref(bool enabled) {
  GetPrefs()->SetBoolean(prefs::kAutoclickEnabled, enabled);
}

bool GetLargeCursorEnabledFromPref() {
  return GetPrefs()->GetBoolean(prefs::kLargeCursorEnabled);
}

bool GetHighContrastEnabledFromPref() {
  return GetPrefs()->GetBoolean(prefs::kHighContrastEnabled);
}

bool GetSpokenFeedbackEnabledFromPref() {
  return GetPrefs()->GetBoolean(prefs::kSpokenFeedbackEnabled);
}

bool GetAutoclickEnabledFromPref() {
  return GetPrefs()->GetBoolean(prefs::kAutoclickEnabled);
}

}  // anonymouse namespace

class AccessibilityManagerTest : public InProcessBrowserTest {
 protected:
  AccessibilityManagerTest() {}
  virtual ~AccessibilityManagerTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(chromeos::switches::kLoginManager);
    command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile,
                                    TestingProfile::kTestUserProfileDir);
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    // Sets the login-screen profile.
    AccessibilityManager::Get()->
        SetProfileForTest(ProfileHelper::GetSigninProfile());
  }

  content::NotificationRegistrar registrar_;
  DISALLOW_COPY_AND_ASSIGN(AccessibilityManagerTest);
};

IN_PROC_BROWSER_TEST_F(AccessibilityManagerTest, Login) {
  // Confirms that a11y features are disabled on the login screen.
  EXPECT_FALSE(IsLargeCursorEnabled());
  EXPECT_FALSE(IsSpokenFeedbackEnabled());
  EXPECT_FALSE(IsHighContrastEnabled());
  EXPECT_FALSE(IsAutoclickEnabled());

  // Logs in.
  UserManager::Get()->UserLoggedIn(kTestUserName, kTestUserName, true);

  // Confirms that the features still disabled just after login.
  EXPECT_FALSE(IsLargeCursorEnabled());
  EXPECT_FALSE(IsSpokenFeedbackEnabled());
  EXPECT_FALSE(IsHighContrastEnabled());
  EXPECT_FALSE(IsAutoclickEnabled());

  UserManager::Get()->SessionStarted();

  // Confirms that the features are still disabled just after login.
  EXPECT_FALSE(IsLargeCursorEnabled());
  EXPECT_FALSE(IsSpokenFeedbackEnabled());
  EXPECT_FALSE(IsHighContrastEnabled());
  EXPECT_FALSE(IsAutoclickEnabled());

  // Enables large cursor.
  SetLargeCursorEnabled(true);
  // Confirms that large cursor is enabled.
  EXPECT_TRUE(IsLargeCursorEnabled());

  // Enables spoken feedback.
  SetSpokenFeedbackEnabled(true);
  // Confirms that the spoken feedback is enabled.
  EXPECT_TRUE(IsSpokenFeedbackEnabled());

  // Enables high contrast.
  SetHighContrastEnabled(true);
  // Confirms that high cotrast is enabled.
  EXPECT_TRUE(IsHighContrastEnabled());

  // Enables autoclick.
  SetAutoclickEnabled(true);
  // Confirms that autoclick is enabled.
  EXPECT_TRUE(IsAutoclickEnabled());
}

IN_PROC_BROWSER_TEST_F(AccessibilityManagerTest, TypePref) {
  // Logs in.
  UserManager::Get()->UserLoggedIn(kTestUserName, kTestUserName, true);
  UserManager::Get()->SessionStarted();

  // Confirms that the features are disabled just after login.
  EXPECT_FALSE(IsLargeCursorEnabled());
  EXPECT_FALSE(IsSpokenFeedbackEnabled());
  EXPECT_FALSE(IsHighContrastEnabled());
  EXPECT_FALSE(IsAutoclickEnabled());

  // Sets the pref as true to enable the large cursor.
  SetLargeCursorEnabledPref(true);
  // Confirms that the large cursor is enabled.
  EXPECT_TRUE(IsLargeCursorEnabled());

  // Sets the pref as true to enable the spoken feedback.
  SetSpokenFeedbackEnabledPref(true);
  // Confirms that the spoken feedback is enabled.
  EXPECT_TRUE(IsSpokenFeedbackEnabled());

  // Sets the pref as true to enable high contrast mode.
  SetHighContrastEnabledPref(true);
  // Confirms that the high contrast mode is enabled.
  EXPECT_TRUE(IsHighContrastEnabled());

  // Sets the pref as true to enable autoclick.
  SetAutoclickEnabledPref(true);
  // Confirms that autoclick is enabled.
  EXPECT_TRUE(IsAutoclickEnabled());

  SetLargeCursorEnabledPref(false);
  EXPECT_FALSE(IsLargeCursorEnabled());

  SetSpokenFeedbackEnabledPref(false);
  EXPECT_FALSE(IsSpokenFeedbackEnabled());

  SetHighContrastEnabledPref(false);
  EXPECT_FALSE(IsHighContrastEnabled());

  SetAutoclickEnabledPref(false);
  EXPECT_FALSE(IsAutoclickEnabled());
}

IN_PROC_BROWSER_TEST_F(AccessibilityManagerTest, ResumeSavedPref) {
  // Loads the profile of the user.
  UserManager::Get()->UserLoggedIn(kTestUserName, kTestUserName, true);

  // Sets the pref to enable large cursor before login.
  SetLargeCursorEnabledPref(true);
  EXPECT_FALSE(IsLargeCursorEnabled());

  // Sets the pref to enable spoken feedback before login.
  SetSpokenFeedbackEnabledPref(true);
  EXPECT_FALSE(IsSpokenFeedbackEnabled());

  // Sets the pref to enable high contrast before login.
  SetHighContrastEnabledPref(true);
  EXPECT_FALSE(IsHighContrastEnabled());

  // Sets the pref to enable autoclick before login.
  SetAutoclickEnabledPref(true);
  EXPECT_FALSE(IsAutoclickEnabled());

  // Logs in.
  UserManager::Get()->SessionStarted();

  // Confirms that features are enabled by restring from pref just after login.
  EXPECT_TRUE(IsLargeCursorEnabled());
  EXPECT_TRUE(IsSpokenFeedbackEnabled());
  EXPECT_TRUE(IsHighContrastEnabled());
  EXPECT_TRUE(IsAutoclickEnabled());
}

IN_PROC_BROWSER_TEST_F(AccessibilityManagerTest,
                       ChangingTypeInvokesNotification) {
  MockAccessibilityObserver observer;

  // Logs in.
  UserManager::Get()->UserLoggedIn(kTestUserName, kTestUserName, true);
  UserManager::Get()->SessionStarted();

  EXPECT_FALSE(observer.observed());
  observer.reset();

  SetSpokenFeedbackEnabled(true);
  EXPECT_TRUE(observer.observed());
  EXPECT_TRUE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(),
            chrome::NOTIFICATION_CROS_ACCESSIBILITY_TOGGLE_SPOKEN_FEEDBACK);
  EXPECT_TRUE(IsSpokenFeedbackEnabled());

  observer.reset();
  SetSpokenFeedbackEnabled(false);
  EXPECT_TRUE(observer.observed());
  EXPECT_FALSE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(),
            chrome::NOTIFICATION_CROS_ACCESSIBILITY_TOGGLE_SPOKEN_FEEDBACK);
  EXPECT_FALSE(IsSpokenFeedbackEnabled());

  observer.reset();
  SetHighContrastEnabled(true);
  EXPECT_TRUE(observer.observed());
  EXPECT_TRUE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(),
            chrome::NOTIFICATION_CROS_ACCESSIBILITY_TOGGLE_HIGH_CONTRAST_MODE);
  EXPECT_TRUE(IsHighContrastEnabled());

  observer.reset();
  SetHighContrastEnabled(false);
  EXPECT_TRUE(observer.observed());
  EXPECT_FALSE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(),
            chrome::NOTIFICATION_CROS_ACCESSIBILITY_TOGGLE_HIGH_CONTRAST_MODE);
  EXPECT_FALSE(IsHighContrastEnabled());
}

IN_PROC_BROWSER_TEST_F(AccessibilityManagerTest,
                       ChangingTypePrefInvokesNotification) {
  MockAccessibilityObserver observer;

  // Logs in.
  UserManager::Get()->UserLoggedIn(kTestUserName, kTestUserName, true);
  UserManager::Get()->SessionStarted();

  EXPECT_FALSE(observer.observed());
  observer.reset();

  SetSpokenFeedbackEnabledPref(true);
  EXPECT_TRUE(observer.observed());
  EXPECT_TRUE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(),
            chrome::NOTIFICATION_CROS_ACCESSIBILITY_TOGGLE_SPOKEN_FEEDBACK);
  EXPECT_TRUE(IsSpokenFeedbackEnabled());

  observer.reset();
  SetSpokenFeedbackEnabledPref(false);
  EXPECT_TRUE(observer.observed());
  EXPECT_FALSE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(),
            chrome::NOTIFICATION_CROS_ACCESSIBILITY_TOGGLE_SPOKEN_FEEDBACK);
  EXPECT_FALSE(IsSpokenFeedbackEnabled());

  observer.reset();
  SetHighContrastEnabledPref(true);
  EXPECT_TRUE(observer.observed());
  EXPECT_TRUE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(),
            chrome::NOTIFICATION_CROS_ACCESSIBILITY_TOGGLE_HIGH_CONTRAST_MODE);
  EXPECT_TRUE(IsHighContrastEnabled());

  observer.reset();
  SetHighContrastEnabledPref(false);
  EXPECT_TRUE(observer.observed());
  EXPECT_FALSE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(),
            chrome::NOTIFICATION_CROS_ACCESSIBILITY_TOGGLE_HIGH_CONTRAST_MODE);
  EXPECT_FALSE(IsHighContrastEnabled());
}

class AccessibilityManagerUserTypeTest
    : public AccessibilityManagerTest,
      public ::testing::WithParamInterface<const char*> {
 protected:
  AccessibilityManagerUserTypeTest() {}
  virtual ~AccessibilityManagerUserTypeTest() {}

  DISALLOW_COPY_AND_ASSIGN(AccessibilityManagerUserTypeTest);
};

// TODO(yoshiki): Enable a test for retail mode.
INSTANTIATE_TEST_CASE_P(
    UserTypeInstantiation,
    AccessibilityManagerUserTypeTest,
    ::testing::Values(kTestUserName,
                      UserManager::kGuestUserName,
                      //UserManager::kRetailModeUserName,
                      kTestLocallyManagedUserName));

IN_PROC_BROWSER_TEST_P(AccessibilityManagerUserTypeTest,
                       EnableOnLoginScreenAndLogin) {
  // Enables large cursor.
  SetLargeCursorEnabled(true);
  EXPECT_TRUE(IsLargeCursorEnabled());
  // Enables spoken feedback.
  SetSpokenFeedbackEnabled(true);
  EXPECT_TRUE(IsSpokenFeedbackEnabled());
  // Enables high contrast.
  SetHighContrastEnabled(true);
  EXPECT_TRUE(IsHighContrastEnabled());
  // Enables autoclick.
  SetAutoclickEnabled(true);
  EXPECT_TRUE(IsAutoclickEnabled());

  // Logs in.
  const char* user_name = GetParam();
  UserManager::Get()->UserLoggedIn(user_name, user_name, true);

  // Confirms that the features are still enabled just after login.
  EXPECT_TRUE(IsLargeCursorEnabled());
  EXPECT_TRUE(IsSpokenFeedbackEnabled());
  EXPECT_TRUE(IsHighContrastEnabled());
  EXPECT_TRUE(IsAutoclickEnabled());

  UserManager::Get()->SessionStarted();

  // Confirms that the features keep enabled after session starts.
  EXPECT_TRUE(IsLargeCursorEnabled());
  EXPECT_TRUE(IsSpokenFeedbackEnabled());
  EXPECT_TRUE(IsHighContrastEnabled());
  EXPECT_TRUE(IsAutoclickEnabled());

  // Confirms that the prefs have been copied to the user's profile.
  EXPECT_TRUE(GetLargeCursorEnabledFromPref());
  EXPECT_TRUE(GetSpokenFeedbackEnabledFromPref());
  EXPECT_TRUE(GetHighContrastEnabledFromPref());
  EXPECT_TRUE(GetAutoclickEnabledFromPref());
}

}  // namespace chromeos
