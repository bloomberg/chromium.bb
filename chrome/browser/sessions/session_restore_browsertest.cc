// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/defaults.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"

typedef InProcessBrowserTest SessionRestoreTest;

// Makes sure when session restore is triggered in the same process we don't end
// up with an extra tab.
IN_PROC_BROWSER_TEST_F(SessionRestoreTest,
                       RestoreOnNewWindowWithNoTabbedBrowsers) {
  if (browser_defaults::kRestorePopups)
    return;

  GURL url(ui_test_utils::GetTestUrl(L".", L"title1.html"));
  ui_test_utils::NavigateToURL(browser(), url);

  // Turn on session restore.
  SessionStartupPref::SetStartupPref(
      browser()->profile(),
      SessionStartupPref(SessionStartupPref::LAST));

  // Create a new popup.
  Profile* profile = browser()->profile();
  Browser* popup = Browser::CreateForPopup(profile);
  popup->window()->Show();

  // Close the browser.
  browser()->window()->Close();

  // Create a new window, which should trigger session restore.
  popup->NewWindow();

  Browser* new_browser = ui_test_utils::WaitForNewBrowser();

  ASSERT_TRUE(new_browser != NULL);

  // The browser should only have one tab.
  ASSERT_EQ(1, new_browser->tab_count());

  // And the first url should be url.
  EXPECT_EQ(url, new_browser->GetTabContentsAt(0)->GetURL());
}
