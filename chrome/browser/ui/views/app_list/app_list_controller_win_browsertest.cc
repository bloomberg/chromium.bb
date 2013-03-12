// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "chrome/browser/ui/app_list/app_list_service_win.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/test/base/in_process_browser_test.h"

class AppListControllerWinTest : public InProcessBrowserTest {
};

// Test the CreateNewWindow function of the controller delegate.
IN_PROC_BROWSER_TEST_F(AppListControllerWinTest, CreateNewWindow) {
  AppListService* service = chrome::GetAppListServiceWin();
  AppListControllerDelegate* controller = service->CreateControllerDelegate();
  chrome::HostDesktopType desktop = chrome::GetActiveDesktop();

  EXPECT_EQ(1U, chrome::GetBrowserCount(browser()->profile(), desktop));
  EXPECT_EQ(0U, chrome::GetBrowserCount(
      browser()->profile()->GetOffTheRecordProfile(), desktop));

  controller->CreateNewWindow(browser()->profile(), false);
  EXPECT_EQ(2U, chrome::GetBrowserCount(browser()->profile(), desktop));

  controller->CreateNewWindow(browser()->profile(), true);
  EXPECT_EQ(1U, chrome::GetBrowserCount(
      browser()->profile()->GetOffTheRecordProfile(), desktop));
}
