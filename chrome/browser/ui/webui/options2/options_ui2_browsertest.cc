// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options2/options_ui2_browsertest.h"

#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace options2 {

OptionsBrowserTest::OptionsBrowserTest() {
  EnableDOMAutomation();
}

void OptionsBrowserTest::NavigateToSettings() {
  const GURL& url = GURL(chrome::kChromeUISettingsURL);
  ui_test_utils::NavigateToURL(browser(), url);
}

void OptionsBrowserTest::VerifyNavbar() {
  bool navbar_exist = false;
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      browser()->GetSelectedWebContents()->GetRenderViewHost(),
      L"",
      L"domAutomationController.send("
      L"!!document.getElementById('navigation'))", &navbar_exist));
  EXPECT_EQ(true, navbar_exist);
}

void OptionsBrowserTest::VerifyTitle() {
  string16 title = browser()->GetSelectedWebContents()->GetTitle();
  string16 expected_title = l10n_util::GetStringUTF16(IDS_SETTINGS_TITLE);
  EXPECT_NE(title.find(expected_title), string16::npos);
}

// Observed flaky on Win and Mac, see http://crbug.com/119671.
#if defined(OS_WIN) || defined(OS_MACOSX)
#define MAYBE_LoadOptionsByURL FLAKY_LoadOptionsByURL
#else
#define MAYBE_LoadOptionsByURL LoadOptionsByURL
#endif

IN_PROC_BROWSER_TEST_F(OptionsBrowserTest, MAYBE_LoadOptionsByURL) {
  NavigateToSettings();
  VerifyTitle();
  VerifyNavbar();
}

}  // namespace options2
