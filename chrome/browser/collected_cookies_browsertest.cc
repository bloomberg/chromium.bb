// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

typedef InProcessBrowserTest CollectedCookiesTest;

// If this crashes on Windows, use http://crbug.com/79331
IN_PROC_BROWSER_TEST_F(CollectedCookiesTest, DoubleDisplay) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  // Disable cookies.
  CookieSettingsFactory::GetForProfile(browser()->profile())
      ->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);

  // Load a page with cookies.
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/cookie1.html"));

  // Click on the info link twice.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  TabDialogs::FromWebContents(web_contents)->ShowCollectedCookies();
  TabDialogs::FromWebContents(web_contents)->ShowCollectedCookies();
}

// If this crashes on Windows, use http://crbug.com/79331
IN_PROC_BROWSER_TEST_F(CollectedCookiesTest, NavigateAway) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  // Disable cookies.
  CookieSettingsFactory::GetForProfile(browser()->profile())
      ->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);

  // Load a page with cookies.
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/cookie1.html"));

  // Click on the info link.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  TabDialogs::FromWebContents(web_contents)->ShowCollectedCookies();

  // Navigate to another page.
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/cookie2.html"));
}
