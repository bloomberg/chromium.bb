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

class MagnificationManagerTest : public CrosInProcessBrowserTest {
 protected:
  MagnificationManagerTest() {}
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

  void SetScreenManagnifierTypeToPref(ash::MagnifierType type) {
    prefs()->SetString(prefs::kMagnifierType,
                       accessibility::ScreenMagnifierNameFromType(type));
  }

  void CheckCurrentMagnifierType(
      ash::MagnifierType type) {
    EXPECT_EQ(MagnificationManager::GetInstance()->GetMagnifierType(),
             type);
  }

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
  SetScreenManagnifierTypeToPref(ash::MAGNIFIER_FULL);

  // Confirms that magnifier is enabled.
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

}  // namespace chromeos
