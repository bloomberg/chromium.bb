// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/test/app_window_waiter.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_app_launcher.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

base::FilePath GetTestDemoAppPath() {
  base::FilePath test_data_dir;
  EXPECT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
  return test_data_dir.Append(FILE_PATH_LITERAL("chromeos/demo_app"));
}

Profile* WaitForProfile() {
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  if (!user_manager || !user_manager->IsUserLoggedIn()) {
    content::WindowedNotificationObserver(
        chrome::NOTIFICATION_SESSION_STARTED,
        content::NotificationService::AllSources())
        .Wait();
  }

  return ProfileManager::GetActiveUserProfile();
}

bool VerifyDemoAppLaunch() {
  Profile* profile = WaitForProfile();
  return apps::AppWindowWaiter(extensions::AppWindowRegistry::Get(profile),
                               DemoAppLauncher::kDemoAppId)
             .Wait() != NULL;
}

bool VerifyNetworksDisabled() {
  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();
  return !handler->DefaultNetwork();
}

}  // namespace

class DemoAppLauncherTest : public ExtensionBrowserTest {
 public:
  DemoAppLauncherTest() { set_exit_when_last_browser_closes(false); }

  ~DemoAppLauncherTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kLoginManager);
    command_line->AppendSwitch(switches::kForceLoginManagerInTests);
    command_line->AppendSwitchASCII(switches::kLoginProfile, "user");

    command_line->AppendSwitchASCII(switches::kDerelictIdleTimeout, "0");
    command_line->AppendSwitchASCII(switches::kOobeTimerInterval, "0");
    command_line->AppendSwitchASCII(switches::kDerelictDetectionTimeout, "0");
  }

  void SetUp() override {
    chromeos::DemoAppLauncher::SetDemoAppPathForTesting(GetTestDemoAppPath());
    ExtensionBrowserTest::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DemoAppLauncherTest);
};

IN_PROC_BROWSER_TEST_F(DemoAppLauncherTest, Basic) {
  // This test is fairly unique in the sense that the test actually starts as
  // soon as Chrome launches, so there isn't any typical "launch this test"
  // steps that we need to take. All we can do is verify that our demo app
  // did launch.
  EXPECT_TRUE(VerifyDemoAppLaunch());
}

IN_PROC_BROWSER_TEST_F(DemoAppLauncherTest, NoNetwork) {
  EXPECT_TRUE(VerifyDemoAppLaunch());
  EXPECT_TRUE(VerifyNetworksDisabled());
}

}  // namespace chromeos
