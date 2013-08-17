// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/cros_settings_names.h"
#include "chrome/browser/policy/proto/chromeos/chrome_device_policy.pb.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/chromeos_switches.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/mock_notification_observer.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace chromeos {

class LoginScreenPolicyTest : public policy::DevicePolicyCrosBrowserTest {
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kLoginManager);
    command_line->AppendSwitch(::switches::kEnableManagedUsers);
  }

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    InstallOwnerKey();
    MarkAsEnterpriseOwned();
    DevicePolicyCrosBrowserTest::SetUpInProcessBrowserTestFixture();
  }
};

IN_PROC_BROWSER_TEST_F(LoginScreenPolicyTest, DisableSupervisedUsers) {
  EXPECT_FALSE(chromeos::UserManager::Get()->AreLocallyManagedUsersAllowed());

  content::WindowedNotificationObserver windowed_observer(
       chrome::NOTIFICATION_SYSTEM_SETTING_CHANGED,
       content::NotificationService::AllSources());
  chromeos::CrosSettings::Get()->AddSettingsObserver(
      chromeos::kAccountsPrefSupervisedUsersEnabled, &windowed_observer);
  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.mutable_supervised_users_settings()->set_supervised_users_enabled(true);
  RefreshDevicePolicy();
  windowed_observer.Wait();

  EXPECT_TRUE(chromeos::UserManager::Get()->AreLocallyManagedUsersAllowed());

  chromeos::CrosSettings::Get()->RemoveSettingsObserver(
      chromeos::kAccountsPrefSupervisedUsersEnabled, &windowed_observer);
}

}  // namespace chromeos
