// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/options_ui_browsertest.h"

#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace options {

OptionsBrowserTest::OptionsBrowserTest() {
}

void OptionsBrowserTest::NavigateToSettings() {
  const GURL& url = GURL(chrome::kChromeUISettingsURL);
  ui_test_utils::NavigateToURL(browser(), url);
}

void OptionsBrowserTest::VerifyNavbar() {
  bool navbar_exist = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      chrome::GetActiveWebContents(browser()),
      "domAutomationController.send("
      "    !!document.getElementById('navigation'))",
      &navbar_exist));
  EXPECT_EQ(true, navbar_exist);
}

void OptionsBrowserTest::VerifyTitle() {
  string16 title = chrome::GetActiveWebContents(browser())->GetTitle();
  string16 expected_title = l10n_util::GetStringUTF16(IDS_SETTINGS_TITLE);
  EXPECT_NE(title.find(expected_title), string16::npos);
}

// Flaky, see http://crbug.com/119671.
IN_PROC_BROWSER_TEST_F(OptionsBrowserTest, DISABLED_LoadOptionsByURL) {
  NavigateToSettings();
  VerifyTitle();
  VerifyNavbar();
}

}  // namespace options
