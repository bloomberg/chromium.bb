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

class MagnificationManagerTest : public CrosInProcessBrowserTest,
                                 public content::NotificationObserver {
 protected:
  MagnificationManagerTest() : observed_(false),
                               observed_type_(ash::MAGNIFIER_OFF) {}
  virtual ~MagnificationManagerTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kLoginManager);
    command_line->AppendSwitchASCII(switches::kLoginProfile,
                                    TestingProfile::kTestUserProfileDir);
  }

  Profile* profile() {
    Profile* profile = ProfileManager::GetDefaultProfileOrOffTheRecord();
    DCHECK(profile);
    return profile;
  }

  PrefServiceBase* prefs() {
    return PrefServiceBase::FromBrowserContext(profile());
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    registrar_.Add(
        this,
        chrome::NOTIFICATION_CROS_ACCESSIBILITY_TOGGLE_SCREEN_MAGNIFIER,
        content::NotificationService::AllSources());
  }

  void SetScreenManagnifierType(ash::MagnifierType type) {
    MagnificationManager::Get()->SetMagnifier(type);
  }

  void SetScreenManagnifierTypeToPref(ash::MagnifierType type) {
    prefs()->SetBoolean(prefs::kScreenMagnifierEnabled,
                        (type != ash::MAGNIFIER_OFF) ? true : false);
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

  void CheckCurrentMagnifierType(
      ash::MagnifierType type) {
    EXPECT_EQ(MagnificationManager::Get()->GetMagnifierType(), type);
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
        observed_type_ = accessibility_status->enabled ? ash::MAGNIFIER_FULL :
                                                         ash::MAGNIFIER_OFF;
        break;
      }
    }
  }

  bool observed_;
  ash::MagnifierType observed_type_;
  content::NotificationRegistrar registrar_;
  DISALLOW_COPY_AND_ASSIGN(MagnificationManagerTest);
};

IN_PROC_BROWSER_TEST_F(MagnificationManagerTest, LoginOffToOff) {
  // Confirms that magnifier is disabled on the login screen.
  CheckCurrentMagnifierType(ash::MAGNIFIER_OFF);

  // Logs in.
  UserManager::Get()->UserLoggedIn("owner@invalid.domain", true);

  // Confirms that magnifier is still disabled just after login.
  CheckCurrentMagnifierType(ash::MAGNIFIER_OFF);

  UserManager::Get()->SessionStarted();

  // Confirms that magnifier is still disabled just after login.
  CheckCurrentMagnifierType(ash::MAGNIFIER_OFF);

  // Enables magnifier.
  SetScreenManagnifierType(ash::MAGNIFIER_FULL);
  // Confirms that magnifier is enabled.
  CheckCurrentMagnifierType(ash::MAGNIFIER_FULL);
}

IN_PROC_BROWSER_TEST_F(MagnificationManagerTest, LoginFullToOff) {
  // Confirms that magnifier is disabled on the login screen.
  CheckCurrentMagnifierType(ash::MAGNIFIER_OFF);

  // Enables magnifier on login scren.
  SetScreenManagnifierType(ash::MAGNIFIER_FULL);

  // Logs in (but the session is not started yet).
  UserManager::Get()->UserLoggedIn("owner@invalid.domain", true);
  // Confirms that magnifier is keeping enabled.
  CheckCurrentMagnifierType(ash::MAGNIFIER_FULL);

  UserManager::Get()->SessionStarted();

  // Confirms that magnifier is disabled just after login.
  CheckCurrentMagnifierType(ash::MAGNIFIER_OFF);
}

IN_PROC_BROWSER_TEST_F(MagnificationManagerTest, LoginOffToFull) {
  // Changes to full screen magnifier again and confirms that.
  SetScreenManagnifierType(ash::MAGNIFIER_OFF);
  CheckCurrentMagnifierType(ash::MAGNIFIER_OFF);

  // Logs in (but the session is not started yet).
  UserManager::Get()->UserLoggedIn("owner@invalid.domain", true);

  // Confirms that magnifier is keeping disabled.
  CheckCurrentMagnifierType(ash::MAGNIFIER_OFF);
  // Enable magnifier on the pref.
  SetScreenManagnifierTypeToPref(ash::MAGNIFIER_FULL);
  SetSavedFullScreenMagnifierScale(2.5);

  UserManager::Get()->SessionStarted();

  // Confirms that the prefs are successfully loaded.
  CheckCurrentMagnifierType(ash::MAGNIFIER_FULL);
  EXPECT_EQ(2.5, GetFullScreenMagnifierScale());
}

IN_PROC_BROWSER_TEST_F(MagnificationManagerTest, LoginFullToFull) {
  // Changes to full screen magnifier again and confirms that.
  SetScreenManagnifierType(ash::MAGNIFIER_FULL);
  CheckCurrentMagnifierType(ash::MAGNIFIER_FULL);

  // Logs in (but the session is not started yet).
  UserManager::Get()->UserLoggedIn("owner@invalid.domain", true);

  // Confirms that magnifier is keeping enabled.
  CheckCurrentMagnifierType(ash::MAGNIFIER_FULL);
  // Enable magnifier on the pref.
  SetScreenManagnifierTypeToPref(ash::MAGNIFIER_FULL);
  SetSavedFullScreenMagnifierScale(2.5);

  UserManager::Get()->SessionStarted();

  // Confirms that the prefs are successfully loaded.
  CheckCurrentMagnifierType(ash::MAGNIFIER_FULL);
  EXPECT_EQ(2.5, GetFullScreenMagnifierScale());
}

IN_PROC_BROWSER_TEST_F(MagnificationManagerTest, ChangeMagnifierType) {
  // Changes to full screen magnifier and confirms that.
  SetScreenManagnifierType(ash::MAGNIFIER_FULL);
  CheckCurrentMagnifierType(ash::MAGNIFIER_FULL);

  // Changes to partial screen magnifier and confirms that.
  SetScreenManagnifierType(ash::MAGNIFIER_PARTIAL);
  CheckCurrentMagnifierType(ash::MAGNIFIER_PARTIAL);

  // Disable magnifier and confirms that.
  SetScreenManagnifierType(ash::MAGNIFIER_OFF);
  CheckCurrentMagnifierType(ash::MAGNIFIER_OFF);

  // Changes to full screen magnifier again and confirms that.
  SetScreenManagnifierType(ash::MAGNIFIER_FULL);
  CheckCurrentMagnifierType(ash::MAGNIFIER_FULL);

  // Logs in
  UserManager::Get()->UserLoggedIn("owner@invalid.domain", true);
  UserManager::Get()->SessionStarted();

  // Changes to full screen magnifier and confirms that.
  SetScreenManagnifierType(ash::MAGNIFIER_FULL);
  CheckCurrentMagnifierType(ash::MAGNIFIER_FULL);

  // Changes to partial screen magnifier and confirms that.
  SetScreenManagnifierType(ash::MAGNIFIER_PARTIAL);
  CheckCurrentMagnifierType(ash::MAGNIFIER_PARTIAL);

  // Disable magnifier and confirms that.
  SetScreenManagnifierType(ash::MAGNIFIER_OFF);
  CheckCurrentMagnifierType(ash::MAGNIFIER_OFF);

  // Changes to full screen magnifier again and confirms that.
  SetScreenManagnifierType(ash::MAGNIFIER_FULL);
  CheckCurrentMagnifierType(ash::MAGNIFIER_FULL);
}

IN_PROC_BROWSER_TEST_F(MagnificationManagerTest, TypePref) {
  // Logs in
  UserManager::Get()->UserLoggedIn("owner@invalid.domain", true);
  UserManager::Get()->SessionStarted();

  // Confirms that magnifier is disabled just after login.
  CheckCurrentMagnifierType(ash::MAGNIFIER_OFF);

  // Sets the pref as true to enable magnifier.
  SetScreenManagnifierTypeToPref(ash::MAGNIFIER_FULL);
  // Confirms that magnifier is enabled.
  CheckCurrentMagnifierType(ash::MAGNIFIER_FULL);

  // Sets the pref as false to disabled magnifier.
  SetScreenManagnifierTypeToPref(ash::MAGNIFIER_OFF);
  // Confirms that magnifier is disabled.
  CheckCurrentMagnifierType(ash::MAGNIFIER_OFF);

  // Sets the pref as true to enable magnifier again.
  SetScreenManagnifierTypeToPref(ash::MAGNIFIER_FULL);
  // Confirms that magnifier is enabled.
  CheckCurrentMagnifierType(ash::MAGNIFIER_FULL);
}

IN_PROC_BROWSER_TEST_F(MagnificationManagerTest, ResumeSavedTypePref) {
  // Loads the profile of the user.
  UserManager::Get()->UserLoggedIn("owner@invalid.domain", true);

  // Sets the pref as true to enable magnifier before login.
  SetScreenManagnifierTypeToPref(ash::MAGNIFIER_FULL);

  // Logs in.
  UserManager::Get()->SessionStarted();

  // Confirms that magnifier is enabled just after login.
  CheckCurrentMagnifierType(ash::MAGNIFIER_FULL);
}

IN_PROC_BROWSER_TEST_F(MagnificationManagerTest, ScalePref) {
  SetScreenManagnifierType(ash::MAGNIFIER_OFF);
  CheckCurrentMagnifierType(ash::MAGNIFIER_OFF);

  // Sets 2.5x to the pref.
  SetSavedFullScreenMagnifierScale(2.5);

  // Enables full screen magnifier.
  SetScreenManagnifierType(ash::MAGNIFIER_FULL);
  CheckCurrentMagnifierType(ash::MAGNIFIER_FULL);

  // Confirms that 2.5x is restored.
  EXPECT_EQ(2.5, GetFullScreenMagnifierScale());

  // Sets the scale and confirms that the scale is saved to pref.
  SetFullScreenMagnifierScale(3.0);
  EXPECT_EQ(3.0, GetSavedFullScreenMagnifierScale());
}

IN_PROC_BROWSER_TEST_F(MagnificationManagerTest, InvalidScalePref) {
  // TEST 1: too small scale
  SetScreenManagnifierType(ash::MAGNIFIER_OFF);
  CheckCurrentMagnifierType(ash::MAGNIFIER_OFF);

  // Sets too small value to the pref.
  SetSavedFullScreenMagnifierScale(0.5);

  // Enables full screen magnifier.
  SetScreenManagnifierType(ash::MAGNIFIER_FULL);
  CheckCurrentMagnifierType(ash::MAGNIFIER_FULL);

  // Confirms that the actual scale is set to the minimum scale.
  EXPECT_EQ(1.0, GetFullScreenMagnifierScale());

  // TEST 2: too large scale
  SetScreenManagnifierType(ash::MAGNIFIER_OFF);
  CheckCurrentMagnifierType(ash::MAGNIFIER_OFF);

  // Sets too large value to the pref.
  SetSavedFullScreenMagnifierScale(50.0);

  // Enables full screen magnifier.
  SetScreenManagnifierType(ash::MAGNIFIER_FULL);
  CheckCurrentMagnifierType(ash::MAGNIFIER_FULL);

  // Confirms that the actual scale is set to the maximum scale.
  EXPECT_EQ(4.0, GetFullScreenMagnifierScale());
}

IN_PROC_BROWSER_TEST_F(MagnificationManagerTest,
                       ChangingTypeInvokesNotification) {
  // Logs in
  UserManager::Get()->UserLoggedIn("owner@invalid.domain", true);
  UserManager::Get()->SessionStarted();

  // Before the test, sets to full magnifier.
  SetScreenManagnifierTypeToPref(ash::MAGNIFIER_FULL);
  CheckCurrentMagnifierType(ash::MAGNIFIER_FULL);

  // Disables magnifier and confirms observer is invoked.
  observed_ = false;
  SetScreenManagnifierTypeToPref(ash::MAGNIFIER_OFF);
  EXPECT_TRUE(observed_);
  EXPECT_EQ(observed_type_, ash::MAGNIFIER_OFF);
  CheckCurrentMagnifierType(ash::MAGNIFIER_OFF);

  // Enables full screen magnifier and confirms observer is invoked.
  observed_ = false;
  SetScreenManagnifierTypeToPref(ash::MAGNIFIER_FULL);
  EXPECT_TRUE(observed_);
  EXPECT_EQ(observed_type_, ash::MAGNIFIER_FULL);
  CheckCurrentMagnifierType(ash::MAGNIFIER_FULL);

  // Disables magnifier again and confirms observer is invoked.
  observed_ = false;
  SetScreenManagnifierTypeToPref(ash::MAGNIFIER_OFF);
  EXPECT_TRUE(observed_);
  EXPECT_EQ(observed_type_, ash::MAGNIFIER_OFF);
  CheckCurrentMagnifierType(ash::MAGNIFIER_OFF);
}

}  // namespace chromeos
