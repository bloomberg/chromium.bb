// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_navigator_browsertest.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/tab_contents/tab_contents.h"

namespace {

GURL GetGoogleURL() {
  return GURL("http://www.google.com/");
}

// Subclass that tests navigation while in the Guest session.
class BrowserGuestSessionNavigatorTest: public BrowserNavigatorTest {
 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) {
    CommandLine command_line_copy = *command_line;
    command_line_copy.AppendSwitchASCII(switches::kLoginProfile, "user");
    chromeos::LoginUtils::Get()->GetOffTheRecordCommandLine(GetGoogleURL(),
                                                            command_line_copy,
                                                            command_line);
  }
};

// This test verifies that the settings page is opened in the incognito window
// in Guest Session (as well as all other windows in Guest session).
IN_PROC_BROWSER_TEST_F(BrowserGuestSessionNavigatorTest,
                       Disposition_Settings_UseIncognitoWindow) {
  Browser* incognito_browser = CreateIncognitoBrowser();

  EXPECT_EQ(2u, BrowserList::size());
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(1, incognito_browser->tab_count());

  // Navigate to the settings page.
  browser::NavigateParams p(MakeNavigateParams(incognito_browser));
  p.disposition = SINGLETON_TAB;
  p.url = GURL("chrome://settings");
  p.window_action = browser::NavigateParams::SHOW_WINDOW;
  p.path_behavior = browser::NavigateParams::IGNORE_AND_NAVIGATE;
  browser::Navigate(&p);

  // Settings page should be opened in incognito window.
  EXPECT_NE(browser(), p.browser);
  EXPECT_EQ(incognito_browser, p.browser);
  EXPECT_EQ(2, incognito_browser->tab_count());
  EXPECT_EQ(GURL("chrome://settings"),
            incognito_browser->GetSelectedTabContents()->GetURL());
}

// This test verifies that navigating to a large window with
// WindowOpenDisposition = NEW_POPUP from a normal Browser results in a new tab.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Disposition_LargePopup) {
  browser::NavigateParams p(MakeNavigateParams());
  p.disposition = NEW_POPUP;
  p.window_bounds = gfx::Rect(0, 0, 10000, 10000);
  ui_test_utils::NavigateToURL(&p);

  // NavigateToURL() should have opened a new tab.
  EXPECT_EQ(browser(), p.browser);

  // We should have one window with two tabs.
  EXPECT_EQ(1u, BrowserList::size());
  EXPECT_EQ(2, browser()->tab_count());
}

// This test verifies that navigating to a large window with
// WindowOpenDisposition = NEW_POPUP from a popup window results in a new tab
// in the parent browser.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Disposition_LargePopupFromPopup) {
  // Open a popup.
  browser::NavigateParams p1(MakeNavigateParams());
  p1.disposition = NEW_POPUP;
  p1.window_bounds = gfx::Rect(0, 0, 200, 200);
  ui_test_utils::NavigateToURL(&p1);

  // Open a large popup from the popup.
  browser::NavigateParams p2(MakeNavigateParams(p1.browser));
  p2.disposition = NEW_POPUP;
  p2.window_bounds = gfx::Rect(0, 0, 10000, 10000);
  ui_test_utils::NavigateToURL(&p2);

  // NavigateToURL() should have opened a new tab in the primary browser.
  EXPECT_EQ(browser(), p2.browser);

  // We should have two windows. browser() should have two tabs.
  EXPECT_EQ(2u, BrowserList::size());
  EXPECT_EQ(2, browser()->tab_count());
}

}  // namespace
