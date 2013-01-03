// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/content_settings/collected_cookies_mac.h"

#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/web_contents_modal_dialog_manager.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/test_utils.h"

typedef InProcessBrowserTest CollectedCookiesMacTest;

IN_PROC_BROWSER_TEST_F(CollectedCookiesMacTest, Close) {
  content::WebContents* web_contents = chrome::GetActiveWebContents(browser());
  WebContentsModalDialogManager* web_contents_modal_dialog_manager =
      WebContentsModalDialogManager::FromWebContents(web_contents);
  EXPECT_EQ(0u, web_contents_modal_dialog_manager->dialog_count());

  // Deletes itself.
  CollectedCookiesMac* dialog =
      new CollectedCookiesMac(chrome::GetActiveWebContents(browser()));
  EXPECT_EQ(1u, web_contents_modal_dialog_manager->dialog_count());

  dialog->PerformClose();
  content::RunAllPendingInMessageLoop();
  EXPECT_EQ(0u, web_contents_modal_dialog_manager->dialog_count());
}

IN_PROC_BROWSER_TEST_F(CollectedCookiesMacTest, Outlets) {
  // Deletes itself.
  CollectedCookiesMac* dialog =
      new CollectedCookiesMac(chrome::GetActiveWebContents(browser()));
  CollectedCookiesWindowController* sheet_controller =
      dialog->sheet_controller();

  EXPECT_TRUE([sheet_controller allowedTreeController]);
  EXPECT_TRUE([sheet_controller blockedTreeController]);
  EXPECT_TRUE([sheet_controller allowedOutlineView]);
  EXPECT_TRUE([sheet_controller blockedOutlineView]);
  EXPECT_TRUE([sheet_controller infoBar]);
  EXPECT_TRUE([sheet_controller infoBarIcon]);
  EXPECT_TRUE([sheet_controller infoBarText]);
  EXPECT_TRUE([sheet_controller tabView]);
  EXPECT_TRUE([sheet_controller blockedScrollView]);
  EXPECT_TRUE([sheet_controller blockedCookiesText]);
  EXPECT_TRUE([sheet_controller cookieDetailsViewPlaceholder]);

  [sheet_controller closeSheet:nil];
  content::RunAllPendingInMessageLoop();
}
