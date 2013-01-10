// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/magnifier/magnification_controller.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/accessibility/accessibility_util.h"
#include "chrome/browser/chromeos/accessibility/magnification_manager.h"
#include "chrome/browser/chromeos/cros/cros_in_process_browser_test.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/user_manager_impl.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

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
  Profile* profile = ProfileManager::GetDefaultProfileOrOffTheRecord();
  DCHECK(profile);
  return profile;
}

PrefServiceBase* prefs() {
  return PrefServiceBase::FromBrowserContext(profile());
}

void EnableScreenManagnifierToPref(bool enabled) {
  prefs()->SetBoolean(prefs::kScreenMagnifierEnabled, enabled);
}

void SetScreenManagnifierTypeToPref(ash::MagnifierType type) {
  prefs()->SetInteger(prefs::kScreenMagnifierType, type);
}

}  // anonymouse namespace

class MagnificationManagerTest : public CrosInProcessBrowserTest,
                                 public content::NotificationObserver {
 protected:
  MagnificationManagerTest() : observed_(false),
                               observed_enabled_(false),
                               observed_type_(ash::kDefaultMagnifierType) {}
  virtual ~MagnificationManagerTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kLoginManager);
    command_line->AppendSwitchASCII(switches::kLoginProfile,
                                    TestingProfile::kTestUserProfileDir);
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    registrar_.Add(
        this,
        chrome::NOTIFICATION_CROS_ACCESSIBILITY_TOGGLE_SCREEN_MAGNIFIER,
        content::NotificationService::AllSources());
  }

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    switch (type) {
      case chrome::NOTIFICATION_CROS_ACCESSIBILITY_TOGGLE_SCREEN_MAGNIFIER: {
        accessibility::AccessibilityStatusEventDetails* accessibility_status =
            content::Details<accessibility::AccessibilityStatusEventDetails>(
                details).ptr();

        observed_ = true;
        observed_enabled_ = accessibility_status->enabled;
        observed_type_ = accessibility_status->magnifier_type;
        break;
      }
    }
  }

  bool observed_;
  bool observed_enabled_;
  ash::MagnifierType observed_type_;
  content::NotificationRegistrar registrar_;
  DISALLOW_COPY_AND_ASSIGN(MagnificationManagerTest);
};

IN_PROC_BROWSER_TEST_F(MagnificationManagerTest, LoginOffToOff) {
  // Confirms that magnifier is disabled on the login screen.
  EXPECT_FALSE(IsMagnifierEnabled());

  // Logs in.
  UserManager::Get()->UserLoggedIn("owner@invalid.domain", true);

  // Confirms that magnifier is still disabled just after login.
  EXPECT_FALSE(IsMagnifierEnabled());

  UserManager::Get()->SessionStarted();

  // Confirms that magnifier is still disabled just after login.
  EXPECT_FALSE(IsMagnifierEnabled());

  // Enables magnifier.
  SetMagnifierEnabled(true);
  // Confirms that magnifier is enabled.
  EXPECT_TRUE(IsMagnifierEnabled());
  EXPECT_EQ(ash::MAGNIFIER_FULL, GetMagnifierType());
}

IN_PROC_BROWSER_TEST_F(MagnificationManagerTest, LoginFullToOff) {
  // Confirms that magnifier is disabled on the login screen.
  EXPECT_FALSE(IsMagnifierEnabled());

  // Enables magnifier on login scren.
  SetMagnifierEnabled(true);

  // Logs in (but the session is not started yet).
  UserManager::Get()->UserLoggedIn("owner@invalid.domain", true);
  // Confirms that magnifier is keeping enabled.
  EXPECT_TRUE(IsMagnifierEnabled());
  EXPECT_EQ(ash::MAGNIFIER_FULL, GetMagnifierType());

  UserManager::Get()->SessionStarted();

  // Confirms that magnifier is disabled just after login.
  EXPECT_FALSE(IsMagnifierEnabled());
}

IN_PROC_BROWSER_TEST_F(MagnificationManagerTest, LoginOffToFull) {
  // Changes to full screen magnifier again and confirms that.
  SetMagnifierEnabled(false);
  EXPECT_FALSE(IsMagnifierEnabled());

  // Logs in (but the session is not started yet).
  UserManager::Get()->UserLoggedIn("owner@invalid.domain", true);

  // Confirms that magnifier is keeping disabled.
  EXPECT_FALSE(IsMagnifierEnabled());
  // Enable magnifier on the pref.
  EnableScreenManagnifierToPref(true);
  SetScreenManagnifierTypeToPref(ash::MAGNIFIER_FULL);
  SetSavedFullScreenMagnifierScale(2.5);

  UserManager::Get()->SessionStarted();

  // Confirms that the prefs are successfully loaded.
  EXPECT_TRUE(IsMagnifierEnabled());
  EXPECT_EQ(ash::MAGNIFIER_FULL, GetMagnifierType());
  EXPECT_EQ(2.5, GetFullScreenMagnifierScale());
}

IN_PROC_BROWSER_TEST_F(MagnificationManagerTest, LoginOffToPartial) {
  // Changes to full screen magnifier again and confirms that.
  SetMagnifierEnabled(false);
  EXPECT_FALSE(IsMagnifierEnabled());

  // Logs in (but the session is not started yet).
  UserManager::Get()->UserLoggedIn("owner@invalid.domain", true);

  // Confirms that magnifier is keeping disabled.
  EXPECT_FALSE(IsMagnifierEnabled());
  // Enable magnifier on the pref.
  EnableScreenManagnifierToPref(true);
  SetScreenManagnifierTypeToPref(ash::MAGNIFIER_PARTIAL);

  UserManager::Get()->SessionStarted();

  // Confirms that the prefs are successfully loaded.
  EXPECT_TRUE(IsMagnifierEnabled());
  EXPECT_EQ(ash::MAGNIFIER_PARTIAL, GetMagnifierType());

  // Full screen magnifier scale is 1.0x since it's 'partial' magnifier.
  EXPECT_EQ(1.0, GetFullScreenMagnifierScale());
}

IN_PROC_BROWSER_TEST_F(MagnificationManagerTest, LoginFullToFull) {
  // Changes to full screen magnifier again and confirms that.
  SetMagnifierType(ash::MAGNIFIER_FULL);
  SetMagnifierEnabled(true);
  EXPECT_TRUE(IsMagnifierEnabled());
  EXPECT_EQ(ash::MAGNIFIER_FULL, GetMagnifierType());

  // Logs in (but the session is not started yet).
  UserManager::Get()->UserLoggedIn("owner@invalid.domain", true);

  // Confirms that magnifier is keeping enabled.
  EXPECT_TRUE(IsMagnifierEnabled());
  EXPECT_EQ(ash::MAGNIFIER_FULL, GetMagnifierType());
  // Enable magnifier on the pref.
  EnableScreenManagnifierToPref(true);
  SetScreenManagnifierTypeToPref(ash::MAGNIFIER_FULL);
  SetSavedFullScreenMagnifierScale(2.5);

  UserManager::Get()->SessionStarted();

  // Confirms that the prefs are successfully loaded.
  EXPECT_TRUE(IsMagnifierEnabled());
  EXPECT_EQ(ash::MAGNIFIER_FULL, GetMagnifierType());
  EXPECT_EQ(2.5, GetFullScreenMagnifierScale());
}

IN_PROC_BROWSER_TEST_F(MagnificationManagerTest, LoginFullToPartial) {
  // Changes to full screen magnifier again and confirms that.
  SetMagnifierType(ash::MAGNIFIER_FULL);
  SetMagnifierEnabled(true);
  EXPECT_TRUE(IsMagnifierEnabled());
  EXPECT_EQ(ash::MAGNIFIER_FULL, GetMagnifierType());

  // Logs in (but the session is not started yet).
  UserManager::Get()->UserLoggedIn("owner@invalid.domain", true);

  // Confirms that magnifier is keeping enabled.
  EXPECT_TRUE(IsMagnifierEnabled());
  EXPECT_EQ(ash::MAGNIFIER_FULL, GetMagnifierType());
  // Enable magnifier on the pref.
  EnableScreenManagnifierToPref(true);
  SetScreenManagnifierTypeToPref(ash::MAGNIFIER_PARTIAL);

  UserManager::Get()->SessionStarted();

  // Confirms that the prefs are successfully loaded.
  EXPECT_TRUE(IsMagnifierEnabled());
  EXPECT_EQ(ash::MAGNIFIER_PARTIAL, GetMagnifierType());

  // Full screen magnifier scale is 1.0x since it's 'partial' magnifier.
  EXPECT_EQ(1.0, GetFullScreenMagnifierScale());
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
  EXPECT_EQ(ash::MAGNIFIER_PARTIAL, GetMagnifierType());

  SetMagnifierEnabled(true);
  EXPECT_TRUE(IsMagnifierEnabled());
  EXPECT_EQ(ash::MAGNIFIER_PARTIAL, GetMagnifierType());

  SetMagnifierEnabled(false);
  EXPECT_FALSE(IsMagnifierEnabled());
  EXPECT_EQ(ash::MAGNIFIER_PARTIAL, GetMagnifierType());

  // Changes the magnifier type when the magnifier is enabled.
  SetMagnifierType(ash::MAGNIFIER_FULL);
  SetMagnifierEnabled(true);
  EXPECT_TRUE(IsMagnifierEnabled());
  EXPECT_EQ(ash::MAGNIFIER_FULL, GetMagnifierType());

  SetMagnifierType(ash::MAGNIFIER_PARTIAL);
  EXPECT_TRUE(IsMagnifierEnabled());
  EXPECT_EQ(ash::MAGNIFIER_PARTIAL, GetMagnifierType());

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
  EXPECT_EQ(ash::MAGNIFIER_PARTIAL, GetMagnifierType());

  SetMagnifierType(ash::MAGNIFIER_FULL);
  EXPECT_FALSE(IsMagnifierEnabled());
  EXPECT_EQ(ash::MAGNIFIER_FULL, GetMagnifierType());
}

IN_PROC_BROWSER_TEST_F(MagnificationManagerTest, TypePref) {
  // Logs in
  UserManager::Get()->UserLoggedIn("owner@invalid.domain", true);
  UserManager::Get()->SessionStarted();

  // Confirms that magnifier is disabled just after login.
  EXPECT_FALSE(IsMagnifierEnabled());

  // Sets the pref as true to enable magnifier.
  SetScreenManagnifierTypeToPref(ash::MAGNIFIER_FULL);
  EnableScreenManagnifierToPref(true);
  // Confirms that magnifier is enabled.
  EXPECT_TRUE(IsMagnifierEnabled());
  EXPECT_EQ(ash::MAGNIFIER_FULL, GetMagnifierType());

  SetScreenManagnifierTypeToPref(ash::MAGNIFIER_PARTIAL);
  EXPECT_TRUE(IsMagnifierEnabled());
  EXPECT_EQ(ash::MAGNIFIER_PARTIAL, GetMagnifierType());

  // Sets the pref as false to disabled magnifier.
  EnableScreenManagnifierToPref(false);
  // Confirms that magnifier is disabled.
  EXPECT_FALSE(IsMagnifierEnabled());

  // Sets the pref as true to enable magnifier again.
  EnableScreenManagnifierToPref(true);
  // Confirms that magnifier is enabled.
  EXPECT_TRUE(IsMagnifierEnabled());
  EXPECT_EQ(ash::MAGNIFIER_PARTIAL, GetMagnifierType());
}

IN_PROC_BROWSER_TEST_F(MagnificationManagerTest, ResumeSavedTypeFullPref) {
  // Loads the profile of the user.
  UserManager::Get()->UserLoggedIn("owner@invalid.domain", true);

  // Sets the pref as true to enable magnifier before login.
  EnableScreenManagnifierToPref(true);
  SetScreenManagnifierTypeToPref(ash::MAGNIFIER_FULL);

  // Logs in.
  UserManager::Get()->SessionStarted();

  // Confirms that magnifier is enabled just after login.
  EXPECT_TRUE(IsMagnifierEnabled());
  EXPECT_EQ(ash::MAGNIFIER_FULL, GetMagnifierType());
}

IN_PROC_BROWSER_TEST_F(MagnificationManagerTest, ResumeSavedTypePartialPref) {
  // Loads the profile of the user.
  UserManager::Get()->UserLoggedIn("owner@invalid.domain", true);

  // Sets the pref as true to enable magnifier before login.
  EnableScreenManagnifierToPref(true);
  SetScreenManagnifierTypeToPref(ash::MAGNIFIER_PARTIAL);

  // Logs in.
  UserManager::Get()->SessionStarted();

  // Confirms that magnifier is enabled just after login.
  EXPECT_TRUE(IsMagnifierEnabled());
  EXPECT_EQ(ash::MAGNIFIER_PARTIAL, GetMagnifierType());
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
  // Logs in
  UserManager::Get()->UserLoggedIn("owner@invalid.domain", true);
  UserManager::Get()->SessionStarted();

  // Enable magnifier (without type)
  EnableScreenManagnifierToPref(true);
  EXPECT_TRUE(observed_);

  // Disables magnifier and confirms observer is invoked.
  observed_ = false;
  SetMagnifierEnabled(false);
  EXPECT_TRUE(observed_);

  // Disables magnifier again and confirms observer is not invoked.
  observed_ = false;
  SetMagnifierEnabled(false);
  EXPECT_FALSE(observed_);

  // Enables full screen magnifier and confirms observer is invoked.
  observed_ = false;
  SetMagnifierType(ash::MAGNIFIER_FULL);
  SetMagnifierEnabled(true);
  EXPECT_TRUE(observed_);

  // Enables full screen magnifier again and confirms observer is invoked.
  observed_ = false;
  SetMagnifierEnabled(true);
  EXPECT_TRUE(observed_);
  EXPECT_TRUE(observed_enabled_);
  EXPECT_EQ(ash::MAGNIFIER_FULL, observed_type_);

  // Switches to partial screen magnifier and confirms observer is invoked.
  observed_ = false;
  SetMagnifierType(ash::MAGNIFIER_PARTIAL);
  EXPECT_TRUE(observed_);
  EXPECT_TRUE(observed_enabled_);
  EXPECT_EQ(ash::MAGNIFIER_PARTIAL, observed_type_);

  // Switches to partial screen magnifier and confirms observer is invoked.
  observed_ = false;
  SetMagnifierType(ash::MAGNIFIER_FULL);
  EXPECT_TRUE(observed_);
  EXPECT_TRUE(observed_enabled_);
  EXPECT_EQ(ash::MAGNIFIER_FULL, observed_type_);

  // Disables magnifier again and confirms observer is invoked.
  observed_ = false;
  SetMagnifierEnabled(false);
  EXPECT_TRUE(observed_);
  EXPECT_FALSE(observed_enabled_);
  EXPECT_FALSE(IsMagnifierEnabled());
}

}  // namespace chromeos
