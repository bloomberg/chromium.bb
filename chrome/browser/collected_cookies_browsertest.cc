// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/content_settings/cookie_settings.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "net/test/spawned_test_server.h"

namespace {

const base::FilePath::CharType kDocRoot[] =
    FILE_PATH_LITERAL("chrome/test/data");

}  // namespace

typedef InProcessBrowserTest CollectedCookiesTest;

// If this crashes on Windows, use http://crbug.com/79331
IN_PROC_BROWSER_TEST_F(CollectedCookiesTest, DoubleDisplay) {
  ASSERT_TRUE(test_server()->Start());

  // Disable cookies.
  CookieSettings::Factory::GetForProfile(browser()->profile())->
      SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);

  // Load a page with cookies.
  ui_test_utils::NavigateToURL(
      browser(), test_server()->GetURL("files/cookie1.html"));

  // Click on the info link twice.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  chrome::ShowCollectedCookiesDialog(web_contents);
  chrome::ShowCollectedCookiesDialog(web_contents);
}

// If this crashes on Windows, use http://crbug.com/79331
IN_PROC_BROWSER_TEST_F(CollectedCookiesTest, NavigateAway) {

  // Disable cookies.
  CookieSettings::Factory::GetForProfile(browser()->profile())->
      SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);

  // Load a page with cookies.
  ui_test_utils::NavigateToURL(
      browser(), test_server()->GetURL("files/cookie1.html"));

  // Click on the info link.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  chrome::ShowCollectedCookiesDialog(web_contents);

  // Navigate to another page.
  ui_test_utils::NavigateToURL(
      browser(), test_server()->GetURL("files/cookie2.html"));
}
