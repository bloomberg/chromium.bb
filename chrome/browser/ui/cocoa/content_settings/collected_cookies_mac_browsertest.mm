// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/content_settings/collected_cookies_mac.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/test/test_utils.h"

using web_modal::WebContentsModalDialogManager;

typedef InProcessBrowserTest CollectedCookiesMacTest;

IN_PROC_BROWSER_TEST_F(CollectedCookiesMacTest, Close) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  WebContentsModalDialogManager* web_contents_modal_dialog_manager =
      WebContentsModalDialogManager::FromWebContents(web_contents);
  EXPECT_FALSE(web_contents_modal_dialog_manager->IsDialogActive());

  // Deletes itself.
  CollectedCookiesMac* dialog = new CollectedCookiesMac(
      browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_TRUE(web_contents_modal_dialog_manager->IsDialogActive());

  dialog->PerformClose();
  content::RunAllPendingInMessageLoop();
  EXPECT_FALSE(web_contents_modal_dialog_manager->IsDialogActive());
}

IN_PROC_BROWSER_TEST_F(CollectedCookiesMacTest, Outlets) {
  // Deletes itself.
  CollectedCookiesMac* dialog = new CollectedCookiesMac(
      browser()->tab_strip_model()->GetActiveWebContents());
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
