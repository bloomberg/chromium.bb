// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/website_settings/permission_bubble_browser_test_util.h"

#include "base/command_line.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/website_settings/mock_permission_bubble_request.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

TestPermissionBubbleViewDelegate::TestPermissionBubbleViewDelegate()
    : PermissionBubbleView::Delegate() {
}

PermissionBubbleBrowserTest::PermissionBubbleBrowserTest()
    : InProcessBrowserTest() {
}

PermissionBubbleBrowserTest::~PermissionBubbleBrowserTest() {
}

void PermissionBubbleBrowserTest::SetUpOnMainThread() {
  InProcessBrowserTest::SetUpOnMainThread();

  // Add a single permission request.
  MockPermissionBubbleRequest* request = new MockPermissionBubbleRequest(
      "Request 1", l10n_util::GetStringUTF8(IDS_PERMISSION_ALLOW),
      l10n_util::GetStringUTF8(IDS_PERMISSION_DENY));
  requests_.push_back(request);
}

PermissionBubbleAppBrowserTest::PermissionBubbleAppBrowserTest()
    : InProcessBrowserTest(),
      PermissionBubbleBrowserTest(),
      ExtensionBrowserTest(),
      app_browser_(nullptr) {
}

PermissionBubbleAppBrowserTest::~PermissionBubbleAppBrowserTest() {
}

void PermissionBubbleAppBrowserTest::SetUpOnMainThread() {
  PermissionBubbleBrowserTest::SetUpOnMainThread();
  ExtensionBrowserTest::SetUpOnMainThread();

  auto extension =
      LoadExtension(test_data_dir_.AppendASCII("app_with_panel_container/"));
  ASSERT_TRUE(extension);

  app_browser_ = OpenExtensionAppWindow(extension);
  ASSERT_TRUE(app_browser());
  ASSERT_TRUE(app_browser()->is_app());
}

void PermissionBubbleAppBrowserTest::SetUp() {
  ExtensionBrowserTest::SetUp();
}

void PermissionBubbleAppBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  ExtensionBrowserTest::SetUpCommandLine(command_line);
}

Browser* PermissionBubbleAppBrowserTest::OpenExtensionAppWindow(
    const extensions::Extension* extension) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kAppId, extension->id());

  AppLaunchParams params(browser()->profile(), extension,
                         extensions::LAUNCH_CONTAINER_PANEL, NEW_WINDOW,
                         extensions::SOURCE_COMMAND_LINE);
  params.command_line = command_line;
  params.current_directory = base::FilePath();

  content::WebContents* app_window = OpenApplication(params);
  assert(app_window);

  return chrome::FindBrowserWithWebContents(app_window);
}

PermissionBubbleKioskBrowserTest::PermissionBubbleKioskBrowserTest()
    : PermissionBubbleBrowserTest() {
}

PermissionBubbleKioskBrowserTest::~PermissionBubbleKioskBrowserTest() {
}

void PermissionBubbleKioskBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  PermissionBubbleBrowserTest::SetUpCommandLine(command_line);
  command_line->AppendSwitch(switches::kKioskMode);
}
