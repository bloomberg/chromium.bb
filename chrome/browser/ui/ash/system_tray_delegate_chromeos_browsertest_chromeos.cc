// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/system_tray_delegate_chromeos.h"

#include <string>
#include <vector>

#include "ash/shell.h"
#include "ash/system/date/date_default_view.h"
#include "ash/system/date/date_view.h"
#include "ash/system/date/tray_date.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/ui/user_adding_screen.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/common/pref_names.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/test_utils.h"

namespace chromeos {

namespace {

const char* kUser1 = "user1@test.com";
const char* kUser2 = "user2@test.com";

base::HourClockType GetHourType() {
  const ash::TrayDate* tray_date = ash::Shell::GetInstance()
                                       ->GetPrimarySystemTray()
                                       ->GetTrayDateForTesting();
  const ash::DateDefaultView* date_default_view =
      tray_date->GetDefaultViewForTesting();

  return date_default_view->GetDateView()->GetHourTypeForTesting();
}

void CreateDefaultView() {
  ash::TrayDate* tray_date = ash::Shell::GetInstance()
                                 ->GetPrimarySystemTray()
                                 ->GetTrayDateForTesting();
  tray_date->CreateDefaultViewForTesting(ash::user::LOGGED_IN_NONE);
}

}  // namespace

class SystemTrayDelegateChromeOSTest : public LoginManagerTest {
 protected:
  SystemTrayDelegateChromeOSTest()
      : LoginManagerTest(false /* should_launch_browser */) {}

  ~SystemTrayDelegateChromeOSTest() override {}

  void SetupUserProfile(std::string user_name, bool use_24_hour_clock) {
    const user_manager::User* user =
        user_manager::UserManager::Get()->FindUser(user_name);
    Profile* profile = ProfileHelper::Get()->GetProfileByUser(user);
    profile->GetPrefs()->SetBoolean(prefs::kUse24HourClock, use_24_hour_clock);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemTrayDelegateChromeOSTest);
};

IN_PROC_BROWSER_TEST_F(SystemTrayDelegateChromeOSTest,
                       PRE_TestMultiProfile24HourClock) {
  RegisterUser(kUser1);
  RegisterUser(kUser2);
  StartupUtils::MarkOobeCompleted();
}

// Test that clock type is taken from user profile for current active user.
IN_PROC_BROWSER_TEST_F(SystemTrayDelegateChromeOSTest,
                       TestMultiProfile24HourClock) {
  LoginUser(kUser1);
  SetupUserProfile(kUser1, true /* use_24_hour_clock */);
  CreateDefaultView();
  EXPECT_EQ(base::k24HourClock, GetHourType());
  UserAddingScreen::Get()->Start();
  content::RunAllPendingInMessageLoop();
  AddUser(kUser2);
  SetupUserProfile(kUser2, false /* use_24_hour_clock */);
  CreateDefaultView();
  EXPECT_EQ(base::k12HourClock, GetHourType());
  user_manager::UserManager::Get()->SwitchActiveUser(kUser1);
  CreateDefaultView();
  EXPECT_EQ(base::k24HourClock, GetHourType());
}

}  // namespace chromeos
