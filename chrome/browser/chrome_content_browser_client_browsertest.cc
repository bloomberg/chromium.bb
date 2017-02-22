// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

namespace content {

class ChromeContentBrowserClientBrowserTest : public InProcessBrowserTest {
 public:
  // Returns the last committed navigation entry of the first tab. May be NULL
  // if there is no such entry.
  NavigationEntry* GetLastCommittedEntry() {
    return browser()->tab_strip_model()->GetWebContentsAt(0)->
        GetController().GetLastCommittedEntry();
  }

  void SetUpInProcessBrowserTestFixture() override {
    disable_md_settings_.InitAndDisableFeature(
        features::kMaterialDesignSettings);
  }

#if defined(OS_CHROMEOS)
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kDisableSettingsWindow);
  }
#endif

 private:
  base::test::ScopedFeatureList disable_md_settings_;
};

IN_PROC_BROWSER_TEST_F(ChromeContentBrowserClientBrowserTest,
                       UberURLHandler_SettingsPage) {
  const GURL url_short("chrome://settings/");
  const GURL url_long("chrome://chrome/settings/");

  ui_test_utils::NavigateToURL(browser(), url_short);
  NavigationEntry* entry = GetLastCommittedEntry();

  ASSERT_TRUE(entry != NULL);
  EXPECT_EQ(url_long, entry->GetURL());
  EXPECT_EQ(url_short, entry->GetVirtualURL());
}

IN_PROC_BROWSER_TEST_F(ChromeContentBrowserClientBrowserTest,
                       UberURLHandler_ContentSettingsPage) {
  const GURL url_short("chrome://settings/content");
  const GURL url_long("chrome://chrome/settings/content");

  ui_test_utils::NavigateToURL(browser(), url_short);
  NavigationEntry* entry = GetLastCommittedEntry();

  ASSERT_TRUE(entry != NULL);
  EXPECT_EQ(url_long, entry->GetURL());
  EXPECT_EQ(url_short, entry->GetVirtualURL());
}

IN_PROC_BROWSER_TEST_F(ChromeContentBrowserClientBrowserTest,
                       UberURLHandler_AboutPage) {
  const GURL url("chrome://chrome/");

  ui_test_utils::NavigateToURL(browser(), url);
  NavigationEntry* entry = GetLastCommittedEntry();

  ASSERT_TRUE(entry != NULL);
  EXPECT_EQ(url, entry->GetURL());
  EXPECT_EQ(url, entry->GetVirtualURL());
}

IN_PROC_BROWSER_TEST_F(ChromeContentBrowserClientBrowserTest,
                       UberURLHandler_NewTabPageOverride) {
  PrefService* prefs = browser()->profile()->GetPrefs();
  static const char kOverrideUrl[] = "http://override.com";
  prefs->SetString(prefs::kNewTabPageLocationOverride, kOverrideUrl);
  const GURL ntp_url(chrome::kChromeUINewTabURL);

  ui_test_utils::NavigateToURL(browser(), ntp_url);
  NavigationEntry* entry = GetLastCommittedEntry();

  ASSERT_TRUE(entry != NULL);
  EXPECT_TRUE(entry->GetVirtualURL().is_valid());
  EXPECT_EQ(GURL(kOverrideUrl), entry->GetVirtualURL());

  prefs->SetString(prefs::kNewTabPageLocationOverride, "");

  ui_test_utils::NavigateToURL(browser(), ntp_url);
  entry = GetLastCommittedEntry();

  ASSERT_TRUE(entry != NULL);
  EXPECT_TRUE(entry->GetVirtualURL().is_valid());
  EXPECT_EQ(ntp_url, entry->GetVirtualURL());
}

IN_PROC_BROWSER_TEST_F(ChromeContentBrowserClientBrowserTest,
                       UberURLHandler_EmptyHost) {
  const GURL url("chrome://chrome//foo");

  ui_test_utils::NavigateToURL(browser(), url);
  NavigationEntry* entry = GetLastCommittedEntry();

  ASSERT_TRUE(entry != NULL);
  EXPECT_TRUE(entry->GetVirtualURL().is_valid());
  EXPECT_EQ(url, entry->GetVirtualURL());
}

// Use a test class with SetUpCommandLine to ensure the flag is sent to the
// first renderer process.
class ChromeContentBrowserClientSitePerProcessTest
    : public ChromeContentBrowserClientBrowserTest {
 public:
  ChromeContentBrowserClientSitePerProcessTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    content::IsolateAllSitesForTesting(command_line);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeContentBrowserClientSitePerProcessTest);
};

// Test that a basic navigation works in --site-per-process mode.  This prevents
// regressions when that mode calls out into the ChromeContentBrowserClient,
// such as http://crbug.com/164223.
IN_PROC_BROWSER_TEST_F(ChromeContentBrowserClientSitePerProcessTest,
                       SitePerProcessNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/title1.html"));

  ui_test_utils::NavigateToURL(browser(), url);
  NavigationEntry* entry = GetLastCommittedEntry();

  ASSERT_TRUE(entry != NULL);
  EXPECT_EQ(url, entry->GetURL());
  EXPECT_EQ(url, entry->GetVirtualURL());
}

}  // namespace content
