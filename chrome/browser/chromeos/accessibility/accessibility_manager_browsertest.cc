// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"

#include "ash/magnifier/magnification_controller.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/accessibility/magnification_manager.h"
#include "chrome/browser/chromeos/cros/cros_in_process_browser_test.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/user_manager_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/chromeos_switches.h"
#include "content/public/browser/notification_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

const char kTestUserName[] = "owner@invalid.domain";

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

Profile* GetProfile() {
  Profile* profile = ProfileManager::GetDefaultProfileOrOffTheRecord();
  DCHECK(profile);
  return profile;
}

PrefService* GetPrefs() {
  return GetProfile()->GetPrefs();
}

void SetLargeCursorEnabledToPref(bool enabled) {
  GetPrefs()->SetBoolean(prefs::kLargeCursorEnabled, enabled);
}

void SetHighContrastEnabledToPref(bool enabled) {
  GetPrefs()->SetBoolean(prefs::kHighContrastEnabled, enabled);
}

void SetSpokenFeedbackEnabledToPref(bool enabled) {
  GetPrefs()->SetBoolean(prefs::kSpokenFeedbackEnabled, enabled);
}

}  // anonymouse namespace

class AccessibilityManagerTest : public CrosInProcessBrowserTest {
 protected:
  AccessibilityManagerTest() {}
  virtual ~AccessibilityManagerTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(chromeos::switches::kLoginManager);
    command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile,
                                    TestingProfile::kTestUserProfileDir);
  }

  virtual void SetUpOnMainThread() OVERRIDE {
  }

  content::NotificationRegistrar registrar_;
  DISALLOW_COPY_AND_ASSIGN(AccessibilityManagerTest);
};

IN_PROC_BROWSER_TEST_F(AccessibilityManagerTest, Login) {
  // Confirms that a11y features are disabled on the login screen.
  EXPECT_FALSE(IsLargeCursorEnabled());
  EXPECT_FALSE(IsSpokenFeedbackEnabled());
  EXPECT_FALSE(IsHighContrastEnabled());

  // Logs in.
  UserManager::Get()->UserLoggedIn(kTestUserName, kTestUserName, true);

  // Confirms that the features still disabled just after login.
  EXPECT_FALSE(IsLargeCursorEnabled());
  EXPECT_FALSE(IsSpokenFeedbackEnabled());
  EXPECT_FALSE(IsHighContrastEnabled());

  UserManager::Get()->SessionStarted();

  // Confirms that the features are still disabled just after login.
  EXPECT_FALSE(IsLargeCursorEnabled());
  EXPECT_FALSE(IsSpokenFeedbackEnabled());
  EXPECT_FALSE(IsHighContrastEnabled());

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
}

IN_PROC_BROWSER_TEST_F(AccessibilityManagerTest, TypePref) {
  // Logs in.
  UserManager::Get()->UserLoggedIn(kTestUserName, kTestUserName, true);
  UserManager::Get()->SessionStarted();

  // Confirms that the features are disabled just after login.
  EXPECT_FALSE(IsLargeCursorEnabled());
  EXPECT_FALSE(IsSpokenFeedbackEnabled());
  EXPECT_FALSE(IsHighContrastEnabled());

  // Sets the pref as true to enable the large cursor.
  SetLargeCursorEnabledToPref(true);
  // Confirms that the large cursor is enabled.
  EXPECT_TRUE(IsLargeCursorEnabled());

  // Sets the pref as true to enable the spoken feedback.
  SetSpokenFeedbackEnabledToPref(true);
  // Confirms that the spoken feedback is enabled.
  EXPECT_TRUE(IsSpokenFeedbackEnabled());

  // Enables the high contrast mode.
  SetHighContrastEnabled(true);
  // Confirms that the high contrast mode is enabled.
  EXPECT_TRUE(IsHighContrastEnabled());

  SetLargeCursorEnabledToPref(false);
  EXPECT_FALSE(IsLargeCursorEnabled());

  SetSpokenFeedbackEnabledToPref(false);
  EXPECT_FALSE(IsSpokenFeedbackEnabled());

  SetHighContrastEnabledToPref(false);
  EXPECT_FALSE(IsHighContrastEnabled());
}

IN_PROC_BROWSER_TEST_F(AccessibilityManagerTest, ResumeSavedPref) {
  // Loads the profile of the user.
  UserManager::Get()->UserLoggedIn(kTestUserName, kTestUserName, true);

  // Sets the pref to enable large cursor before login.
  SetLargeCursorEnabledToPref(true);
  EXPECT_FALSE(IsLargeCursorEnabled());

  // Sets the pref to enable spoken feedback before login.
  SetSpokenFeedbackEnabledToPref(true);
  EXPECT_FALSE(IsSpokenFeedbackEnabled());

  // Sets the pref to enable high contrast before login.
  SetHighContrastEnabledToPref(true);
  EXPECT_FALSE(IsHighContrastEnabled());

  // Logs in.
  UserManager::Get()->SessionStarted();

  // Confirms that features are enabled by restring from pref just after login.
  EXPECT_TRUE(IsLargeCursorEnabled());
  EXPECT_TRUE(IsSpokenFeedbackEnabled());
  EXPECT_TRUE(IsHighContrastEnabled());
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

  SetSpokenFeedbackEnabledToPref(true);
  EXPECT_TRUE(observer.observed());
  EXPECT_TRUE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(),
            chrome::NOTIFICATION_CROS_ACCESSIBILITY_TOGGLE_SPOKEN_FEEDBACK);
  EXPECT_TRUE(IsSpokenFeedbackEnabled());

  observer.reset();
  SetSpokenFeedbackEnabledToPref(false);
  EXPECT_TRUE(observer.observed());
  EXPECT_FALSE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(),
            chrome::NOTIFICATION_CROS_ACCESSIBILITY_TOGGLE_SPOKEN_FEEDBACK);
  EXPECT_FALSE(IsSpokenFeedbackEnabled());

  observer.reset();
  SetHighContrastEnabledToPref(true);
  EXPECT_TRUE(observer.observed());
  EXPECT_TRUE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(),
            chrome::NOTIFICATION_CROS_ACCESSIBILITY_TOGGLE_HIGH_CONTRAST_MODE);
  EXPECT_TRUE(IsHighContrastEnabled());

  observer.reset();
  SetHighContrastEnabledToPref(false);
  EXPECT_TRUE(observer.observed());
  EXPECT_FALSE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(),
            chrome::NOTIFICATION_CROS_ACCESSIBILITY_TOGGLE_HIGH_CONTRAST_MODE);
  EXPECT_FALSE(IsHighContrastEnabled());
}

}  // namespace chromeos
