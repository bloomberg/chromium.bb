// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/engagement/site_engagement_score.h"
#include "chrome/browser/engagement/site_engagement_service.h"
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

namespace {

enum class FeatureTestState { kDisabled, kEnabledWithoutUI, kEnabledWithUI };

struct SiteEngagementTestCase {
  const char* const navigated;
  const char* const suggested;
} kSiteEngagementTestCases[] = {
    {"sité1.test", "site1.test"},
    {"mail.www.sité1.test", "site1.test"},

    // These should match since the comparison uses eTLD+1s.
    {"sité2.test", "www.site2.test"},
    {"mail.sité2.test", "www.site2.test"},

    {"síté3.test", "sité3.test"},
    {"mail.síté3.test", "sité3.test"},

    {"síté4.test", "www.sité4.test"},
    {"mail.síté4.test", "www.sité4.test"},
};

}  // namespace

class IdnNavigationObserverBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<FeatureTestState> {
 protected:
  void SetUp() override {
    if (GetParam() == FeatureTestState::kEnabledWithoutUI) {
      feature_list_.InitAndEnableFeatureWithParameters(
          features::kIdnNavigationSuggestions, {{"metrics_only", "true"}});
    } else if (GetParam() == FeatureTestState::kEnabledWithUI) {
      feature_list_.InitAndEnableFeature(features::kIdnNavigationSuggestions);
    }
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  // Sets the absolute Site Engagement |score| for the testing origin.
  void SetSiteEngagementScore(const GURL& url, double score) {
    SiteEngagementService::Get(browser()->profile())
        ->ResetBaseScoreForURL(url, score);
  }

  void TestInfobarNotShown(const GURL& navigated_url) {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    InfoBarService* infobar_service =
        InfoBarService::FromWebContents(web_contents);

    content::TestNavigationObserver navigation_observer(web_contents, 1);
    ui_test_utils::NavigateToURL(browser(), navigated_url);
    navigation_observer.Wait();
    EXPECT_EQ(0u, infobar_service->infobar_count());
  }

  void TestInfobarShown(const GURL& navigated_url,
                        const GURL& expected_suggested_url) {
    // Sanity check navigated_url.
    url_formatter::IDNConversionResult result =
        url_formatter::IDNToUnicodeWithDetails(navigated_url.host_piece());
    ASSERT_TRUE(result.has_idn_component);

    history::HistoryService* const history_service =
        HistoryServiceFactory::GetForProfile(
            browser()->profile(), ServiceAccessType::EXPLICIT_ACCESS);
    ui_test_utils::WaitForHistoryToLoad(history_service);

    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    InfoBarService* infobar_service =
        InfoBarService::FromWebContents(web_contents);
    InfoBarObserver infobar_added_observer(
        infobar_service, InfoBarObserver::Type::kInfoBarAdded);
    ui_test_utils::NavigateToURL(browser(), navigated_url);
    infobar_added_observer.Wait();

    infobars::InfoBar* infobar = infobar_service->infobar_at(0);
    EXPECT_EQ(infobars::InfoBarDelegate::ALTERNATE_NAV_INFOBAR_DELEGATE,
              infobar->delegate()->GetIdentifier());

    // Clicking the link in the infobar should remove the infobar and navigate
    // to the suggested URL.
    InfoBarObserver infobar_removed_observer(
        infobar_service, InfoBarObserver::Type::kInfoBarRemoved);
    AlternateNavInfoBarDelegate* infobar_delegate =
        static_cast<AlternateNavInfoBarDelegate*>(infobar->delegate());
    infobar_delegate->LinkClicked(WindowOpenDisposition::CURRENT_TAB);
    infobar_removed_observer.Wait();

    EXPECT_EQ(0u, infobar_service->infobar_count());
    EXPECT_EQ(expected_suggested_url, web_contents->GetURL());

    // Clicking the link in the infobar should also remove the original URL from
    // history.
    ui_test_utils::HistoryEnumerator enumerator(browser()->profile());
    EXPECT_FALSE(base::ContainsValue(enumerator.urls(), navigated_url));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_CASE_P(,
                        IdnNavigationObserverBrowserTest,
                        ::testing::Values(FeatureTestState::kDisabled,
                                          FeatureTestState::kEnabledWithoutUI,
                                          FeatureTestState::kEnabledWithUI));

// Navigating to a non-IDN shouldn't show an infobar.
IN_PROC_BROWSER_TEST_P(IdnNavigationObserverBrowserTest, NonIdn_NoInfobar) {
  TestInfobarNotShown(
      embedded_test_server()->GetURL("google.com", "/title1.html"));
}

// Navigating to a domain whose visual representation does not look like a
// top domain shouldn't show an infobar.
IN_PROC_BROWSER_TEST_P(IdnNavigationObserverBrowserTest,
                       NonTopDomainIdn_NoInfobar) {
  TestInfobarNotShown(
      embedded_test_server()->GetURL("éxample.com", "/title1.html"));
}

// Navigating to a domain whose visual representation looks like a top domain
// should show a "Did you mean to go to ..." infobar and record metrics.
IN_PROC_BROWSER_TEST_P(IdnNavigationObserverBrowserTest, TopDomainIdn_Infobar) {
  if (GetParam() != FeatureTestState::kEnabledWithUI)
    return;

  base::HistogramTester histograms;

  TestInfobarShown(embedded_test_server()->GetURL(
                       "googlé.com", "/title1.html") /* navigated */,
                   embedded_test_server()->GetURL(
                       "google.com", "/title1.html") /* suggested */);

  histograms.ExpectTotalCount(IdnNavigationObserver::kHistogramName, 3);
  histograms.ExpectBucketCount(
      IdnNavigationObserver::kHistogramName,
      IdnNavigationObserver::NavigationSuggestionEvent::kInfobarShown, 1);
  histograms.ExpectBucketCount(
      IdnNavigationObserver::kHistogramName,
      IdnNavigationObserver::NavigationSuggestionEvent::kLinkClicked, 1);
  histograms.ExpectBucketCount(
      IdnNavigationObserver::kHistogramName,
      IdnNavigationObserver::NavigationSuggestionEvent::kMatchTopSite, 1);
}

// Same as TopDomainIdn_Infobar but the UI is disabled, so only checks for
// metrics.
IN_PROC_BROWSER_TEST_P(IdnNavigationObserverBrowserTest,
                       TopDomainIdn_Metrics_NoInfobar) {
  if (GetParam() != FeatureTestState::kEnabledWithoutUI)
    return;

  base::HistogramTester histograms;

  TestInfobarNotShown(
      embedded_test_server()->GetURL("googlé.com", "/title1.html"));

  histograms.ExpectTotalCount(IdnNavigationObserver::kHistogramName, 1);
  histograms.ExpectBucketCount(
      IdnNavigationObserver::kHistogramName,
      IdnNavigationObserver::NavigationSuggestionEvent::kMatchTopSite, 1);
}

// Same as TopDomainIdn_Infobar but the user has engaged with the domain before.
// Shouldn't show an infobar.
IN_PROC_BROWSER_TEST_P(IdnNavigationObserverBrowserTest,
                       TopDomainIdn_EngagedSite_NoInfobar) {
  // If the user already engaged with the site, the infobar shouldn't be shown.
  const GURL url = embedded_test_server()->GetURL("googlé.com", "/title1.html");
  SetSiteEngagementScore(url, 20);
  TestInfobarNotShown(url);
}

// Navigating to a domain whose visual representation looks like a domain with a
// site engagement score above a certain threshold should show a "Did you mean
// to go to ..." infobar.
IN_PROC_BROWSER_TEST_P(IdnNavigationObserverBrowserTest,
                       SiteEngagement_Infobar) {
  if (GetParam() != FeatureTestState::kEnabledWithUI)
    return;

  SetSiteEngagementScore(GURL("http://site1.test"), 20);
  SetSiteEngagementScore(GURL("http://www.site2.test"), 20);
  SetSiteEngagementScore(GURL("http://sité3.test"), 20);
  SetSiteEngagementScore(GURL("http://www.sité4.test"), 20);

  for (const auto& test_case : kSiteEngagementTestCases) {
    base::HistogramTester histograms;
    TestInfobarShown(
        embedded_test_server()->GetURL(test_case.navigated, "/title1.html"),
        embedded_test_server()->GetURL(test_case.suggested, "/title1.html"));
    histograms.ExpectTotalCount(IdnNavigationObserver::kHistogramName, 3);
    histograms.ExpectBucketCount(
        IdnNavigationObserver::kHistogramName,
        IdnNavigationObserver::NavigationSuggestionEvent::kInfobarShown, 1);
    histograms.ExpectBucketCount(
        IdnNavigationObserver::kHistogramName,
        IdnNavigationObserver::NavigationSuggestionEvent::kLinkClicked, 1);
    histograms.ExpectBucketCount(
        IdnNavigationObserver::kHistogramName,
        IdnNavigationObserver::NavigationSuggestionEvent::kMatchSiteEngagement,
        1);
  }
}

// Same as SiteEngagement_Infobar but the UI is disabled, so only checks for
// metrics.
IN_PROC_BROWSER_TEST_P(IdnNavigationObserverBrowserTest,
                       SiteEngagement_Metrics_NoInfobar) {
  if (GetParam() != FeatureTestState::kEnabledWithoutUI)
    return;

  SetSiteEngagementScore(GURL("http://site1.test"), 20);
  SetSiteEngagementScore(GURL("http://www.site2.test"), 20);
  SetSiteEngagementScore(GURL("http://sité3.test"), 20);
  SetSiteEngagementScore(GURL("http://www.sité4.test"), 20);

  for (const auto& test_case : kSiteEngagementTestCases) {
    base::HistogramTester histograms;
    TestInfobarNotShown(
        embedded_test_server()->GetURL(test_case.navigated, "/title1.html"));
    histograms.ExpectTotalCount(IdnNavigationObserver::kHistogramName, 1);
    histograms.ExpectBucketCount(
        IdnNavigationObserver::kHistogramName,
        IdnNavigationObserver::NavigationSuggestionEvent::kMatchSiteEngagement,
        1);
  }
}

// The infobar shouldn't be shown when the feature is disabled.
IN_PROC_BROWSER_TEST_P(IdnNavigationObserverBrowserTest,
                       TopDomainIdn_FeatureDisabled) {
  if (GetParam() != FeatureTestState::kDisabled)
    return;

  TestInfobarNotShown(
      embedded_test_server()->GetURL("googlé.com", "/title1.html"));
}
