// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/search/instant_test_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/resource/resource_bundle.h"

class LocalNTPTest : public InProcessBrowserTest,
                     public InstantTestBase {
 public:
  LocalNTPTest() {}

 protected:
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    ASSERT_TRUE(https_test_server().Start());
    GURL instant_url = https_test_server().GetURL(
        "files/instant_extended.html?strk=1&");
    GURL ntp_url = https_test_server().GetURL(
        "files/local_ntp_browsertest.html?strk=1&");
    InstantTestBase::Init(instant_url, ntp_url, false);
  }
};

// Flaky: crbug.com/267117
IN_PROC_BROWSER_TEST_F(LocalNTPTest, DISABLED_LocalNTPJavascriptTest) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmnibox();

  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      instant_url(),
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(chrome::IsInstantNTP(active_tab));
  bool success = false;
  ASSERT_TRUE(GetBoolFromJS(active_tab, "!!runTests()", &success));
  EXPECT_TRUE(success);
}

// Flaky.
IN_PROC_BROWSER_TEST_F(LocalNTPTest,
                       DISABLED_NTPRespectsBrowserLanguageSetting) {
  // Make sure the default language is not French.
  std::string default_locale = g_browser_process->GetApplicationLocale();
  EXPECT_NE("fr", default_locale);

  // Switch browser language to French.
  std::string loaded_locale =
      ui::ResourceBundle::GetSharedInstance().ReloadLocaleResources("fr");
  EXPECT_EQ("fr", loaded_locale);
  g_browser_process->SetApplicationLocale(loaded_locale);
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetString(prefs::kApplicationLocale, loaded_locale);

  // Setup Instant.
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmnibox();

  // Open a new tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(chrome::kChromeUINewTabURL),
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  // Verify that the NTP is in French.
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(base::ASCIIToUTF16("Nouvel onglet"), active_tab->GetTitle());
}
