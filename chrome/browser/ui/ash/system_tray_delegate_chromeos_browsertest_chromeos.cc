// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/system_tray_delegate_chromeos.h"

#include <string>

#include "ash/common/system/date/date_default_view.h"
#include "ash/common/system/date/date_view.h"
#include "ash/common/system/date/system_info_default_view.h"
#include "ash/common/system/date/tray_date.h"
#include "ash/common/system/date/tray_system_info.h"
#include "ash/common/system/tray/system_tray.h"
#include "ash/shell.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/ui/user_adding_screen.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/test_utils.h"

namespace chromeos {

namespace {

// Because policy is not needed this test it is better to use e-mails that
// are definitely not enterprise. This lets us to avoid faking of policy fetch
// procedure.
const char kUser1[] = "user1@gmail.com";
const char kUser2[] = "user2@gmail.com";

base::HourClockType GetHourType() {
  const ash::TraySystemInfo* tray_system_info =
      ash::Shell::GetInstance()
          ->GetPrimarySystemTray()
          ->GetTraySystemInfoForTesting();
  const ash::SystemInfoDefaultView* system_info_default_view =
      tray_system_info->GetDefaultViewForTesting();

  return system_info_default_view->GetDateView()->GetHourTypeForTesting();
}

void CreateDefaultView() {
  ash::TraySystemInfo* tray_system_info = ash::Shell::GetInstance()
                                              ->GetPrimarySystemTray()
                                              ->GetTraySystemInfoForTesting();
  tray_system_info->CreateDefaultViewForTesting(
      ash::LoginStatus::NOT_LOGGED_IN);
}

}  // namespace

class SystemTrayDelegateChromeOSTest : public LoginManagerTest {
 protected:
  SystemTrayDelegateChromeOSTest()
      : LoginManagerTest(false /* should_launch_browser */),
        account_id1_(AccountId::FromUserEmail(kUser1)),
        account_id2_(AccountId::FromUserEmail(kUser2)) {}

  ~SystemTrayDelegateChromeOSTest() override {}

  void SetupUserProfile(const AccountId& account_id, bool use_24_hour_clock) {
    const user_manager::User* user =
        user_manager::UserManager::Get()->FindUser(account_id);
    Profile* profile = ProfileHelper::Get()->GetProfileByUser(user);
    profile->GetPrefs()->SetBoolean(prefs::kUse24HourClock, use_24_hour_clock);
    // Allow clock setting to be sent to ash over mojo.
    content::RunAllPendingInMessageLoop();
  }

  const AccountId account_id1_;
  const AccountId account_id2_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemTrayDelegateChromeOSTest);
};

IN_PROC_BROWSER_TEST_F(SystemTrayDelegateChromeOSTest,
                       PRE_TestMultiProfile24HourClock) {
  RegisterUser(account_id1_.GetUserEmail());
  RegisterUser(account_id2_.GetUserEmail());
  StartupUtils::MarkOobeCompleted();
}

// Test that clock type is taken from user profile for current active user.
IN_PROC_BROWSER_TEST_F(SystemTrayDelegateChromeOSTest,
                       TestMultiProfile24HourClock) {
  LoginUser(account_id1_.GetUserEmail());
  SetupUserProfile(account_id1_, true /* Use_24_hour_clock. */);
  CreateDefaultView();
  EXPECT_EQ(base::k24HourClock, GetHourType());

  UserAddingScreen::Get()->Start();
  content::RunAllPendingInMessageLoop();
  AddUser(account_id2_.GetUserEmail());
  SetupUserProfile(account_id2_, false /* Use_24_hour_clock. */);
  CreateDefaultView();
  EXPECT_EQ(base::k12HourClock, GetHourType());

  user_manager::UserManager::Get()->SwitchActiveUser(account_id1_);
  // Allow clock setting to be sent to ash over mojo.
  content::RunAllPendingInMessageLoop();
  CreateDefaultView();
  EXPECT_EQ(base::k24HourClock, GetHourType());
}

}  // namespace chromeos
