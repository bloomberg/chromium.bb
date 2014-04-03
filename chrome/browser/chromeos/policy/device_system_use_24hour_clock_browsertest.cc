// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"
#include "ash/system/date/date_default_view.h"
#include "ash/system/date/date_view.h"
#include "ash/system/user/login_status.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/browser/chromeos/policy/proto/chrome_device_policy.pb.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/ui/ash/system_tray_delegate_chromeos.h"
#include "chromeos/chromeos_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace chromeos {

class SystemUse24HourClockPolicyTest
    : public policy::DevicePolicyCrosBrowserTest {
 public:
  SystemUse24HourClockPolicyTest() {
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kLoginManager);
    command_line->AppendSwitch(chromeos::switches::kForceLoginManagerInTests);
  }

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    InstallOwnerKey();
    MarkAsEnterpriseOwned();
    DevicePolicyCrosBrowserTest::SetUpInProcessBrowserTestFixture();
  }

 protected:
  void RefreshPolicyAndWaitDeviceSettingsUpdated() {
    scoped_ptr<CrosSettings::ObserverSubscription> observer =
        CrosSettings::Get()->AddSettingsObserver(
            kSystemUse24HourClock,
            base::MessageLoop::current()->QuitWhenIdleClosure());

    RefreshDevicePolicy();
    base::MessageLoop::current()->Run();
  }

  static bool GetSystemTrayDelegateShouldUse24HourClock() {
    chromeos::SystemTrayDelegateChromeOS* tray_delegate =
        static_cast<chromeos::SystemTrayDelegateChromeOS*>(
            ash::Shell::GetInstance()->system_tray_delegate());
    return tray_delegate->GetShouldUse24HourClockForTesting();
  }

  static base::HourClockType TestGetPrimarySystemTrayTimeHourType() {
    const ash::TrayDate* tray_date = ash::Shell::GetInstance()
                                         ->GetPrimarySystemTray()
                                         ->GetTrayDateForTesting();
    const ash::tray::TimeView* time_tray = tray_date->GetTimeTrayForTesting();

    return time_tray->GetHourTypeForTesting();
  }

  static bool TestPrimarySystemTrayHasDateDefaultView() {
    const ash::TrayDate* tray_date = ash::Shell::GetInstance()
                                         ->GetPrimarySystemTray()
                                         ->GetTrayDateForTesting();
    const ash::DateDefaultView* date_default_view =
        tray_date->GetDefaultViewForTesting();
    return (date_default_view != NULL);
  }

  static void TestPrimarySystemTrayCreateDefaultView() {
    ash::TrayDate* tray_date = ash::Shell::GetInstance()
                                   ->GetPrimarySystemTray()
                                   ->GetTrayDateForTesting();
    tray_date->CreateDefaultViewForTesting(ash::user::LOGGED_IN_NONE);
  }

  static base::HourClockType TestGetPrimarySystemTrayDateHourType() {
    const ash::TrayDate* tray_date = ash::Shell::GetInstance()
                                         ->GetPrimarySystemTray()
                                         ->GetTrayDateForTesting();
    const ash::DateDefaultView* date_default_view =
        tray_date->GetDefaultViewForTesting();

    return date_default_view->GetDateView()->GetHourTypeForTesting();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemUse24HourClockPolicyTest);
};

IN_PROC_BROWSER_TEST_F(SystemUse24HourClockPolicyTest, CheckUnset) {
  bool system_use_24hour_clock;
  EXPECT_FALSE(CrosSettings::Get()->GetBoolean(kSystemUse24HourClock,
                                               &system_use_24hour_clock));

  EXPECT_FALSE(GetSystemTrayDelegateShouldUse24HourClock());
  EXPECT_EQ(base::k12HourClock, TestGetPrimarySystemTrayTimeHourType());
  EXPECT_FALSE(TestPrimarySystemTrayHasDateDefaultView());

  TestPrimarySystemTrayCreateDefaultView();
  EXPECT_EQ(base::k12HourClock, TestGetPrimarySystemTrayDateHourType());
}

IN_PROC_BROWSER_TEST_F(SystemUse24HourClockPolicyTest, CheckTrue) {
  bool system_use_24hour_clock = true;
  EXPECT_FALSE(CrosSettings::Get()->GetBoolean(kSystemUse24HourClock,
                                               &system_use_24hour_clock));
  EXPECT_FALSE(TestPrimarySystemTrayHasDateDefaultView());

  EXPECT_FALSE(GetSystemTrayDelegateShouldUse24HourClock());
  EXPECT_EQ(base::k12HourClock, TestGetPrimarySystemTrayTimeHourType());
  TestPrimarySystemTrayCreateDefaultView();
  EXPECT_EQ(base::k12HourClock, TestGetPrimarySystemTrayDateHourType());

  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.mutable_use_24hour_clock()->set_use_24hour_clock(true);
  RefreshPolicyAndWaitDeviceSettingsUpdated();

  system_use_24hour_clock = false;
  EXPECT_TRUE(CrosSettings::Get()->GetBoolean(kSystemUse24HourClock,
                                              &system_use_24hour_clock));
  EXPECT_TRUE(system_use_24hour_clock);
  EXPECT_TRUE(GetSystemTrayDelegateShouldUse24HourClock());
  EXPECT_EQ(base::k24HourClock, TestGetPrimarySystemTrayTimeHourType());

  EXPECT_TRUE(TestPrimarySystemTrayHasDateDefaultView());
  EXPECT_EQ(base::k24HourClock, TestGetPrimarySystemTrayDateHourType());
}

IN_PROC_BROWSER_TEST_F(SystemUse24HourClockPolicyTest, CheckFalse) {
  bool system_use_24hour_clock = true;
  EXPECT_FALSE(CrosSettings::Get()->GetBoolean(kSystemUse24HourClock,
                                               &system_use_24hour_clock));
  EXPECT_FALSE(TestPrimarySystemTrayHasDateDefaultView());

  EXPECT_FALSE(GetSystemTrayDelegateShouldUse24HourClock());
  EXPECT_EQ(base::k12HourClock, TestGetPrimarySystemTrayTimeHourType());
  TestPrimarySystemTrayCreateDefaultView();
  EXPECT_EQ(base::k12HourClock, TestGetPrimarySystemTrayDateHourType());

  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.mutable_use_24hour_clock()->set_use_24hour_clock(false);
  RefreshPolicyAndWaitDeviceSettingsUpdated();

  system_use_24hour_clock = true;
  EXPECT_TRUE(CrosSettings::Get()->GetBoolean(kSystemUse24HourClock,
                                              &system_use_24hour_clock));
  EXPECT_FALSE(system_use_24hour_clock);
  EXPECT_FALSE(GetSystemTrayDelegateShouldUse24HourClock());
  EXPECT_EQ(base::k12HourClock, TestGetPrimarySystemTrayTimeHourType());
  EXPECT_TRUE(TestPrimarySystemTrayHasDateDefaultView());
  EXPECT_EQ(base::k12HourClock, TestGetPrimarySystemTrayDateHourType());
}

}  // namespace chromeos
