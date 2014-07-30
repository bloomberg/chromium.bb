// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "chrome/common/chrome_switches.h"
#include "extensions/common/extension.h"
#include "ui/app_list/app_list_switches.h"

namespace {

// The path of the test application within the "platform_apps" directory.
const char kCustomLauncherPagePath[] = "custom_launcher_page";

// The app ID and URL of the test application.
const char kCustomLauncherPageUrl[] =
    "chrome-extension://lmadimbbgapmngbiclpjjngmdickadpl/main.html";

}  // namespace

// Browser tests for custom launcher pages, platform apps that run as a page in
// the app launcher. Within this test class, LoadAndLaunchPlatformApp runs the
// app inside the launcher, not as a standalone background page.
// the app launcher.
class CustomLauncherPageBrowserTest
    : public extensions::PlatformAppBrowserTest {
 public:
  CustomLauncherPageBrowserTest() {}

  virtual void SetUpCommandLine(base::CommandLine* command_line) OVERRIDE {
    PlatformAppBrowserTest::SetUpCommandLine(command_line);

    // Custom launcher pages only work in the experimental app list.
    command_line->AppendSwitch(app_list::switches::kEnableExperimentalAppList);
    command_line->AppendSwitchASCII(switches::kCustomLauncherPage,
                                    kCustomLauncherPageUrl);
  }

  // Open the launcher. Ignores the Extension argument (this will simply
  // activate any loaded launcher pages).
  virtual void LaunchPlatformApp(
      const extensions::Extension* /*unused*/) OVERRIDE {
    AppListService* service =
        AppListService::Get(chrome::HOST_DESKTOP_TYPE_NATIVE);
    DCHECK(service);
    service->ShowForProfile(browser()->profile());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CustomLauncherPageBrowserTest);
};

IN_PROC_BROWSER_TEST_F(CustomLauncherPageBrowserTest, LoadPageAndOpenLauncher) {
  LoadAndLaunchPlatformApp(kCustomLauncherPagePath, "Launched");
}
