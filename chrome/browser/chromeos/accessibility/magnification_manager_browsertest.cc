// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
                                 public MagnificationObserver {
 protected:
  MagnificationManagerTest() : observed_(false),
                               observed_type_(ash::MAGNIFIER_OFF) {}
  virtual ~MagnificationManagerTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kLoginManager);
    command_line->AppendSwitchASCII(switches::kLoginProfile,
                                    TestingProfile::kTestUserProfileDir);
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    MagnificationManager::Get()->AddObserver(this);
  }

  virtual void CleanUpOnMainThread() OVERRIDE {
    MagnificationManager::Get()->RemoveObserver(this);
  }

  // Overridden from MagnificationObserever:
  virtual void OnMagnifierTypeChanged(ash::MagnifierType new_type) OVERRIDE {
    observed_ = true;
    observed_type_ = new_type;
  }

  Profile* profile() {
    Profile* profile = ProfileManager::GetDefaultProfileOrOffTheRecord();
    DCHECK(profile);
    return profile;
  }

  PrefServiceBase* prefs() {
    return PrefServiceBase::FromBrowserContext(profile());
  }

  void SetScreenManagnifierType(ash::MagnifierType type) {
    MagnificationManager::Get()->SetMagnifier(type);
  }

  void SetScreenManagnifierTypeToPref(ash::MagnifierType type) {
    prefs()->SetString(prefs::kMagnifierType,
                       accessibility::ScreenMagnifierNameFromType(type));
  }

  void CheckCurrentMagnifierType(
      ash::MagnifierType type) {
    EXPECT_EQ(MagnificationManager::Get()->GetMagnifierType(), type);
  }

  bool observed_;
  ash::MagnifierType observed_type_;
  DISALLOW_COPY_AND_ASSIGN(MagnificationManagerTest);
};

IN_PROC_BROWSER_TEST_F(MagnificationManagerTest, Login) {
  // Confirms that magnifier is disabled on the login screen.
  CheckCurrentMagnifierType(ash::MAGNIFIER_OFF);

  // Logs in.
  UserManager::Get()->UserLoggedIn("owner@invalid.domain", true);
  UserManager::Get()->SessionStarted();

  // Confirms that magnifier is still disabled just after login.
  CheckCurrentMagnifierType(ash::MAGNIFIER_OFF);

  // Enables magnifier.
  SetScreenManagnifierType(ash::MAGNIFIER_FULL);

  // Confirms that magnifier is enabled.
  CheckCurrentMagnifierType(ash::MAGNIFIER_FULL);
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

IN_PROC_BROWSER_TEST_F(MagnificationManagerTest, WorkingWithPref) {
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

IN_PROC_BROWSER_TEST_F(MagnificationManagerTest, ResumeSavedPref) {
  // Loads the profile of the user.
  UserManager::Get()->UserLoggedIn("owner@invalid.domain", true);

  // Sets the pref as true to enable magnifier before login.
  SetScreenManagnifierTypeToPref(ash::MAGNIFIER_FULL);

  // Logs in.
  UserManager::Get()->SessionStarted();

  // Confirms that magnifier is enabled just after login.
  CheckCurrentMagnifierType(ash::MAGNIFIER_FULL);
}

IN_PROC_BROWSER_TEST_F(MagnificationManagerTest, ChangingTypeInvokesObserver) {
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

  // Enables partial screen magnifier and confirms observer is invoked.
  observed_ = false;
  SetScreenManagnifierTypeToPref(ash::MAGNIFIER_PARTIAL);
  EXPECT_TRUE(observed_);
  EXPECT_EQ(observed_type_, ash::MAGNIFIER_PARTIAL);
  CheckCurrentMagnifierType(ash::MAGNIFIER_PARTIAL);

  // Disables magnifier again and confirms observer is invoked.
  observed_ = false;
  SetScreenManagnifierTypeToPref(ash::MAGNIFIER_OFF);
  EXPECT_TRUE(observed_);
  EXPECT_EQ(observed_type_, ash::MAGNIFIER_OFF);
  CheckCurrentMagnifierType(ash::MAGNIFIER_OFF);
}

}  // namespace chromeos
