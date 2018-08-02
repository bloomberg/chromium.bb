// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/history_test_utils.h"
#include "chrome/browser/infobars/infobar_observer.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/omnibox/alternate_nav_infobar_delegate.h"
#include "chrome/browser/ui/omnibox/idn_navigation_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/window_open_disposition.h"

class IdnNavigationObserverBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<bool> {
 protected:
  void SetUp() override {
    if (GetParam())
      feature_list_.InitAndEnableFeature(features::kIdnNavigationSuggestions);
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_CASE_P(,
                        IdnNavigationObserverBrowserTest,
                        ::testing::Values(false, true));

// Navigating to a non-IDN shouldn't show an infobar.
IN_PROC_BROWSER_TEST_P(IdnNavigationObserverBrowserTest, NonIdn_NoInfobar) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);
  content::TestNavigationObserver navigation_observer(web_contents, 1);
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("google.com", "/title1.html"));
  navigation_observer.Wait();
  EXPECT_EQ(0u, infobar_service->infobar_count());
}

// Navigating to a domain whose visual representation does not look like a
// top domain shouldn't show an infobar.
IN_PROC_BROWSER_TEST_P(IdnNavigationObserverBrowserTest,
                       NonTopDomainIdn_NoInfobar) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);

  content::TestNavigationObserver navigation_observer(web_contents, 1);
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("éxample.com", "/title1.html"));
  EXPECT_EQ(0u, infobar_service->infobar_count());
}

// Navigating to a domain whose visual representation looks like a top domain
// should show a "Did you mean to go to ..." infobar.
IN_PROC_BROWSER_TEST_P(IdnNavigationObserverBrowserTest, TopDomainIdn_Infobar) {
  if (!GetParam())
    return;

  base::HistogramTester histograms;

  history::HistoryService* const history_service =
      HistoryServiceFactory::GetForProfile(browser()->profile(),
                                           ServiceAccessType::EXPLICIT_ACCESS);
  ui_test_utils::WaitForHistoryToLoad(history_service);

  const GURL kIdnUrl =
      embedded_test_server()->GetURL("googlé.com", "/title1.html");
  const GURL kSuggestedUrl =
      embedded_test_server()->GetURL("google.com", "/title1.html");

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);
  InfoBarObserver infobar_added_observer(infobar_service,
                                         InfoBarObserver::Type::kInfoBarAdded);
  ui_test_utils::NavigateToURL(browser(), kIdnUrl);
  infobar_added_observer.Wait();

  infobars::InfoBar* infobar = infobar_service->infobar_at(0);
  EXPECT_EQ(infobars::InfoBarDelegate::ALTERNATE_NAV_INFOBAR_DELEGATE,
            infobar->delegate()->GetIdentifier());

  // Clicking the link in the infobar should remove the infobar and navigate to
  // the suggested URL.
  InfoBarObserver infobar_removed_observer(
      infobar_service, InfoBarObserver::Type::kInfoBarRemoved);
  AlternateNavInfoBarDelegate* infobar_delegate =
      static_cast<AlternateNavInfoBarDelegate*>(infobar->delegate());
  infobar_delegate->LinkClicked(WindowOpenDisposition::CURRENT_TAB);
  infobar_removed_observer.Wait();

  EXPECT_EQ(0u, infobar_service->infobar_count());
  EXPECT_EQ(kSuggestedUrl, web_contents->GetURL());

  // Clicking the link in the infobar should also remove the original URL from
  // history.
  ui_test_utils::HistoryEnumerator enumerator(browser()->profile());
  EXPECT_FALSE(base::ContainsValue(enumerator.urls(), kIdnUrl));

  histograms.ExpectTotalCount(IdnNavigationObserver::kHistogramName, 2);
  histograms.ExpectBucketCount(
      IdnNavigationObserver::kHistogramName,
      IdnNavigationObserver::NavigationSuggestionEvent::kInfobarShown, 1);
  histograms.ExpectBucketCount(
      IdnNavigationObserver::kHistogramName,
      IdnNavigationObserver::NavigationSuggestionEvent::kLinkClicked, 1);
}

// The infobar shouldn't be shown when the feature is disabled.
IN_PROC_BROWSER_TEST_P(IdnNavigationObserverBrowserTest,
                       TopDomainIdn_FeatureDisabled) {
  if (GetParam())
    return;

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);

  content::TestNavigationObserver navigation_observer(web_contents, 1);
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("googlé.com", "/title1.html"));
  EXPECT_EQ(0u, infobar_service->infobar_count());
}
