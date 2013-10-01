// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
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

  void InstallTemplateURLWithNewTabPage(GURL new_tab_page_url) {
    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(browser()->profile());
    ui_test_utils::WaitForTemplateURLServiceToLoad(template_url_service);

    TemplateURLData data;
    data.SetURL("http://foo.com/url?bar={searchTerms}");
    data.new_tab_url = new_tab_page_url.spec();
    TemplateURL* template_url = new TemplateURL(browser()->profile(), data);
    // Takes ownership.
    template_url_service->Add(template_url);
    template_url_service->SetDefaultSearchProvider(template_url);
  }
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
                       UberURLHandler_EmptyHost) {
  const GURL url("chrome://chrome//foo");

  ui_test_utils::NavigateToURL(browser(), url);
  NavigationEntry* entry = GetLastCommittedEntry();

  ASSERT_TRUE(entry != NULL);
  EXPECT_TRUE(entry->GetVirtualURL().is_valid());
  EXPECT_EQ(url, entry->GetVirtualURL());
}

class InstantNTPURLRewriteTest : public ChromeContentBrowserClientBrowserTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    // browsertest initially spins up a non-Instant renderer for about:blank.
    // In a real browser, navigating this renderer to the Instant new tab page
    // forks a new privileged Instant render process, but that warps
    // browsertest's fragile little mind; it just never navigates. We aren't
    // trying to test the process model here, so turn it off.
    CommandLine::ForCurrentProcess()->AppendSwitch(switches::kSingleProcess);
  }
};

IN_PROC_BROWSER_TEST_F(InstantNTPURLRewriteTest,
                       UberURLHandler_InstantExtendedNewTabPage) {
  const GURL url_original("chrome://newtab");
  const GURL url_rewritten("http://example.com/newtab");
  InstallTemplateURLWithNewTabPage(url_rewritten);
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial("InstantExtended",
      "Group1 use_cacheable_ntp:1"));
  chrome::EnableInstantExtendedAPIForTesting();

  ui_test_utils::NavigateToURL(browser(), url_original);
  NavigationEntry* entry = GetLastCommittedEntry();

  ASSERT_TRUE(entry != NULL);
  EXPECT_EQ(url_rewritten, entry->GetURL());
  EXPECT_EQ(url_original, entry->GetVirtualURL());
}

IN_PROC_BROWSER_TEST_F(ChromeContentBrowserClientBrowserTest,
                       UberURLHandler_InstantExtendedNewTabPageDisabled) {
  // Don't do the kSingleProcess shenanigans here (see the dual test) because
  // otherwise RenderViewImpl crashes in a paranoid fit on startup.
  const GURL url_original("chrome://newtab");
  const GURL url_rewritten("http://example.com/newtab");
  InstallTemplateURLWithNewTabPage(url_rewritten);
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial("InstantExtended",
      "Group1 use_cacheable_ntp:1"));
  chrome::DisableInstantExtendedAPIForTesting();

  ui_test_utils::NavigateToURL(browser(), url_original);
  NavigationEntry* entry = GetLastCommittedEntry();

  ASSERT_TRUE(entry != NULL);
  EXPECT_EQ(url_original, entry->GetURL());
  EXPECT_EQ(url_original, entry->GetVirtualURL());
}

// Test that a basic navigation works in --site-per-process mode.  This prevents
// regressions when that mode calls out into the ChromeContentBrowserClient,
// such as http://crbug.com/164223.
IN_PROC_BROWSER_TEST_F(ChromeContentBrowserClientBrowserTest,
                       SitePerProcessNavigation) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kSitePerProcess);
  ASSERT_TRUE(test_server()->Start());
  const GURL url(test_server()->GetURL("files/title1.html"));

  ui_test_utils::NavigateToURL(browser(), url);
  NavigationEntry* entry = GetLastCommittedEntry();

  ASSERT_TRUE(entry != NULL);
  EXPECT_EQ(url, entry->GetURL());
  EXPECT_EQ(url, entry->GetVirtualURL());
}

}  // namespace content
