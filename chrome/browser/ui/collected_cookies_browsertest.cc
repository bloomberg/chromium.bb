// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/ui_base_features.h"

class CollectedCookiesTest : public DialogBrowserTest {
 public:
  CollectedCookiesTest() {}

  // DialogBrowserTest:
  void ShowDialog(const std::string& name) override {
    ASSERT_TRUE(embedded_test_server()->Start());

    // Disable cookies.
    CookieSettingsFactory::GetForProfile(browser()->profile())
        ->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);

    // Load a page with cookies.
    ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL("/cookie1.html"));

    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    TabDialogs::FromWebContents(web_contents)->ShowCollectedCookies();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CollectedCookiesTest);
};

// Runs with --secondary-ui-md. Users of this can switch to CollectedCookiesTest
// when that is the default.
class CollectedCookiesTestMd : public CollectedCookiesTest {
 public:
  CollectedCookiesTestMd() {}

  // CollectedCookiesTest:
  void SetUp() override {
    UseMdOnly();
    CollectedCookiesTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(CollectedCookiesTestMd);
};

// Test that calls ShowDialog("default"). Interactive when run via
// browser_tests --gtest_filter=BrowserDialogTest.Invoke --interactive
// --dialog=CollectedCookiesTestMd.InvokeDialog_default
IN_PROC_BROWSER_TEST_F(CollectedCookiesTestMd, InvokeDialog_default) {
  RunDialog();
}

// If this crashes on Windows, use http://crbug.com/79331
IN_PROC_BROWSER_TEST_F(CollectedCookiesTest, DoubleDisplay) {
  ShowDialog(std::string());

  // Click on the info link a second time.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  TabDialogs::FromWebContents(web_contents)->ShowCollectedCookies();
}

// If this crashes on Windows, use http://crbug.com/79331
IN_PROC_BROWSER_TEST_F(CollectedCookiesTest, NavigateAway) {
  ShowDialog(std::string());

  // Navigate to another page.
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/cookie2.html"));
}
