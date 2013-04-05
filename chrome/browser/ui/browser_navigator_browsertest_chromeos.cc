// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_navigator_browsertest.h"

#include "base/command_line.h"
#include "chrome/browser/chromeos/login/chrome_restart_request.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"

namespace {

GURL GetGoogleURL() {
  return GURL("http://www.google.com/");
}

// Subclass that tests navigation while in the Guest session.
class BrowserGuestSessionNavigatorTest: public BrowserNavigatorTest {
 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    CommandLine command_line_copy = *command_line;
    command_line_copy.AppendSwitchASCII(switches::kLoginProfile, "user");
    chromeos::GetOffTheRecordCommandLine(GetGoogleURL(),
                                         command_line_copy,
                                         command_line);
  }
};

// This test verifies that the settings page is opened in the incognito window
// in Guest Session (as well as all other windows in Guest session).
IN_PROC_BROWSER_TEST_F(BrowserGuestSessionNavigatorTest,
                       Disposition_Settings_UseIncognitoWindow) {
  Browser* incognito_browser = CreateIncognitoBrowser();

  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, incognito_browser->tab_strip_model()->count());

  // Navigate to the settings page.
  chrome::NavigateParams p(MakeNavigateParams(incognito_browser));
  p.disposition = SINGLETON_TAB;
  p.url = GURL("chrome://chrome/settings");
  p.window_action = chrome::NavigateParams::SHOW_WINDOW;
  p.path_behavior = chrome::NavigateParams::IGNORE_AND_NAVIGATE;
  chrome::Navigate(&p);

  // Settings page should be opened in incognito window.
  EXPECT_NE(browser(), p.browser);
  EXPECT_EQ(incognito_browser, p.browser);
  EXPECT_EQ(2, incognito_browser->tab_strip_model()->count());
  EXPECT_EQ(GURL("chrome://chrome/settings"),
            incognito_browser->tab_strip_model()->GetActiveWebContents()->
                GetURL());
}

}  // namespace
