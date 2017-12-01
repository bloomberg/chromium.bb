// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/i18n/base_i18n_switches.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/search/instant_test_utils.h"
#include "chrome/browser/ui/search/local_ntp_test_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/WebKit/public/platform/web_feature.mojom.h"
#include "url/gurl.h"

class LocalNTPTest : public InProcessBrowserTest {
 public:
  LocalNTPTest() {}

  // Navigates the active tab to chrome://newtab and waits until the NTP is
  // fully loaded. Note that simply waiting for a navigation is not enough,
  // since the MV iframe receives the tiles asynchronously.
  void NavigateToNTPAndWaitUntilLoaded() {
    content::WebContents* active_tab =
        browser()->tab_strip_model()->GetActiveWebContents();

    // Attach a message queue *before* navigating to the NTP, to make sure we
    // don't miss the 'loaded' message due to some race condition.
    content::DOMMessageQueue msg_queue(active_tab);

    // Navigate to the NTP.
    ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
    ASSERT_TRUE(search::IsInstantNTP(active_tab));
    ASSERT_EQ(GURL(chrome::kChromeSearchLocalNtpUrl),
              active_tab->GetController().GetVisibleEntry()->GetURL());

    // At this point, the MV iframe may or may not have been fully loaded. Once
    // it loads, it sends a 'loaded' postMessage to the page. Check if the page
    // has already received that, and if not start listening for it. It's
    // important that these two things happen in the same JS invocation, since
    // otherwise we might miss the message.
    bool mv_tiles_loaded = false;
    ASSERT_TRUE(instant_test_utils::GetBoolFromJS(active_tab,
                                                  R"js(
        (function() {
          if (tilesAreLoaded) {
            return true;
          }
          window.addEventListener('message', function(event) {
            if (event.data.cmd == 'loaded') {
              domAutomationController.send('NavigateToNTPAndWaitUntilLoaded');
            }
          });
          return false;
        })()
                                                  )js",
                                                  &mv_tiles_loaded));

    std::string message;
    // Get rid of the message that the GetBoolFromJS call produces.
    ASSERT_TRUE(msg_queue.PopMessage(&message));

    if (mv_tiles_loaded) {
      // The tiles are already loaded, i.e. we missed the 'loaded' message. All
      // is well.
      return;
    }

    // Not loaded yet. Wait for the "NavigateToNTPAndWaitUntilLoaded" message.
    ASSERT_TRUE(msg_queue.WaitForMessage(&message));
    ASSERT_EQ("\"NavigateToNTPAndWaitUntilLoaded\"", message);
    // There shouldn't be any other messages.
    ASSERT_FALSE(msg_queue.PopMessage(&message));
  }

 private:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(features::kUseGoogleLocalNtp);
    InProcessBrowserTest::SetUp();
  }

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(LocalNTPTest, EmbeddedSearchAPIOnlyAvailableOnNTP) {
  // Set up a test server, so we have some arbitrary non-NTP URL to navigate to.
  net::EmbeddedTestServer test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  test_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(test_server.Start());
  const GURL other_url = test_server.GetURL("/simple.html");

  // Open an NTP.
  content::WebContents* active_tab = local_ntp_test_utils::OpenNewTab(
      browser(), GURL(chrome::kChromeUINewTabURL));
  ASSERT_TRUE(search::IsInstantNTP(active_tab));
  // Check that the embeddedSearch API is available.
  bool result = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "!!window.chrome.embeddedSearch", &result));
  EXPECT_TRUE(result);

  // Navigate somewhere else in the same tab.
  content::TestNavigationObserver elsewhere_observer(active_tab);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), other_url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  elsewhere_observer.Wait();
  ASSERT_TRUE(elsewhere_observer.last_navigation_succeeded());
  ASSERT_FALSE(search::IsInstantNTP(active_tab));

  // Now the embeddedSearch API should have gone away.
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "!!window.chrome.embeddedSearch", &result));
  EXPECT_FALSE(result);

  // Navigate back to the NTP.
  content::TestNavigationObserver back_observer(active_tab);
  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
  back_observer.Wait();
  // The API should be back.
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "!!window.chrome.embeddedSearch", &result));
  EXPECT_TRUE(result);

  // Navigate forward to the non-NTP page.
  content::TestNavigationObserver fwd_observer(active_tab);
  chrome::GoForward(browser(), WindowOpenDisposition::CURRENT_TAB);
  fwd_observer.Wait();
  // The API should be gone.
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "!!window.chrome.embeddedSearch", &result));
  EXPECT_FALSE(result);

  // Navigate to a new NTP instance.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL),
      WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  ASSERT_TRUE(search::IsInstantNTP(active_tab));
  // Now the API should be available again.
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "!!window.chrome.embeddedSearch", &result));
  EXPECT_TRUE(result);
}

// Regression test for crbug.com/776660 and crbug.com/776655.
IN_PROC_BROWSER_TEST_F(LocalNTPTest, EmbeddedSearchAPIExposesStaticFunctions) {
  // Open an NTP.
  content::WebContents* active_tab = local_ntp_test_utils::OpenNewTab(
      browser(), GURL(chrome::kChromeUINewTabURL));
  ASSERT_TRUE(search::IsInstantNTP(active_tab));

  struct TestCase {
    const char* function_name;
    const char* args;
  } test_cases[] = {
      {"window.chrome.embeddedSearch.searchBox.paste", "\"text\""},
      {"window.chrome.embeddedSearch.searchBox.startCapturingKeyStrokes", ""},
      {"window.chrome.embeddedSearch.searchBox.stopCapturingKeyStrokes", ""},
      {"window.chrome.embeddedSearch.newTabPage.checkIsUserSignedIntoChromeAs",
       "\"user@email.com\""},
      {"window.chrome.embeddedSearch.newTabPage.checkIsUserSyncingHistory",
       "\"user@email.com\""},
      {"window.chrome.embeddedSearch.newTabPage.deleteMostVisitedItem", "1"},
      {"window.chrome.embeddedSearch.newTabPage.deleteMostVisitedItem",
       "\"1\""},
      {"window.chrome.embeddedSearch.newTabPage.getMostVisitedItemData", "1"},
      {"window.chrome.embeddedSearch.newTabPage.logEvent", "1"},
      {"window.chrome.embeddedSearch.newTabPage.undoAllMostVisitedDeletions",
       ""},
      {"window.chrome.embeddedSearch.newTabPage.undoMostVisitedDeletion", "1"},
      {"window.chrome.embeddedSearch.newTabPage.undoMostVisitedDeletion",
       "\"1\""},
  };

  for (const TestCase& test_case : test_cases) {
    // Make sure that the API function exists.
    bool result = false;
    ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
        active_tab, base::StringPrintf("!!%s", test_case.function_name),
        &result));
    ASSERT_TRUE(result);

    // Check that it can be called normally.
    EXPECT_TRUE(content::ExecuteScript(
        active_tab,
        base::StringPrintf("%s(%s)", test_case.function_name, test_case.args)));

    // Check that it can be called even after it's assigned to a var, i.e.
    // without a "this" binding.
    EXPECT_TRUE(content::ExecuteScript(
        active_tab,
        base::StringPrintf("var f = %s; f(%s)", test_case.function_name,
                           test_case.args)));
  }
}

IN_PROC_BROWSER_TEST_F(LocalNTPTest, NTPRespectsBrowserLanguageSetting) {
  // If the platform cannot load the French locale (GetApplicationLocale() is
  // platform specific, and has been observed to fail on a small number of
  // platforms), abort the test.
  if (!local_ntp_test_utils::SwitchBrowserLanguageToFrench()) {
    LOG(ERROR) << "Failed switching to French language, aborting test.";
    return;
  }

  // Open a new tab.
  content::WebContents* active_tab = local_ntp_test_utils::OpenNewTab(
      browser(), GURL(chrome::kChromeUINewTabURL));

  // Verify that the NTP is in French.
  EXPECT_EQ(base::ASCIIToUTF16("Nouvel onglet"), active_tab->GetTitle());
}

IN_PROC_BROWSER_TEST_F(LocalNTPTest, GoogleNTPLoadsWithoutError) {
  // Open a new blank tab.
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  ASSERT_FALSE(search::IsInstantNTP(active_tab));

  // Attach a console observer, listening for any message ("*" pattern).
  content::ConsoleObserverDelegate console_observer(active_tab, "*");
  active_tab->SetDelegate(&console_observer);

  base::HistogramTester histograms;

  // Navigate to the NTP.
  NavigateToNTPAndWaitUntilLoaded();

  bool is_google = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "!!window.configData && !!window.configData.isGooglePage",
      &is_google));
  EXPECT_TRUE(is_google);

  // We shouldn't have gotten any console error messages.
  EXPECT_TRUE(console_observer.message().empty()) << console_observer.message();

  // Make sure load time metrics were recorded.
  histograms.ExpectTotalCount("NewTabPage.LoadTime", 1);
  histograms.ExpectTotalCount("NewTabPage.LoadTime.LocalNTP", 1);
  histograms.ExpectTotalCount("NewTabPage.LoadTime.LocalNTP.Google", 1);
  histograms.ExpectTotalCount("NewTabPage.LoadTime.MostVisited", 1);
  histograms.ExpectTotalCount("NewTabPage.TilesReceivedTime", 1);
  histograms.ExpectTotalCount("NewTabPage.TilesReceivedTime.LocalNTP", 1);
  histograms.ExpectTotalCount("NewTabPage.TilesReceivedTime.MostVisited", 1);

  // Make sure impression metrics were recorded. There should be 2 tiles, the
  // default prepopulated TopSites (see history::PrepopulatedPage).
  histograms.ExpectTotalCount("NewTabPage.NumberOfTiles", 1);
  histograms.ExpectBucketCount("NewTabPage.NumberOfTiles", 2, 1);
  histograms.ExpectTotalCount("NewTabPage.SuggestionsImpression", 2);
  histograms.ExpectBucketCount("NewTabPage.SuggestionsImpression", 0, 1);
  histograms.ExpectBucketCount("NewTabPage.SuggestionsImpression", 1, 1);
  histograms.ExpectTotalCount("NewTabPage.SuggestionsImpression.client", 2);
  histograms.ExpectTotalCount("NewTabPage.SuggestionsImpression.Thumbnail", 2);
  histograms.ExpectTotalCount("NewTabPage.TileTitle", 2);
  histograms.ExpectTotalCount("NewTabPage.TileTitle.client", 2);
  histograms.ExpectTotalCount("NewTabPage.TileType", 2);
  histograms.ExpectTotalCount("NewTabPage.TileType.client", 2);
}

IN_PROC_BROWSER_TEST_F(LocalNTPTest, NonGoogleNTPLoadsWithoutError) {
  local_ntp_test_utils::SetUserSelectedDefaultSearchProvider(
      browser()->profile(), "https://www.example.com",
      /*ntp_url=*/"");

  // Open a new blank tab.
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  ASSERT_FALSE(search::IsInstantNTP(active_tab));

  // Attach a console observer, listening for any message ("*" pattern).
  content::ConsoleObserverDelegate console_observer(active_tab, "*");
  active_tab->SetDelegate(&console_observer);

  base::HistogramTester histograms;

  // Navigate to the NTP.
  NavigateToNTPAndWaitUntilLoaded();

  bool is_google = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "!!window.configData && !!window.configData.isGooglePage",
      &is_google));
  EXPECT_FALSE(is_google);

  // We shouldn't have gotten any console error messages.
  EXPECT_TRUE(console_observer.message().empty()) << console_observer.message();

  // Make sure load time metrics were recorded.
  histograms.ExpectTotalCount("NewTabPage.LoadTime", 1);
  histograms.ExpectTotalCount("NewTabPage.LoadTime.LocalNTP", 1);
  histograms.ExpectTotalCount("NewTabPage.LoadTime.LocalNTP.Other", 1);
  histograms.ExpectTotalCount("NewTabPage.LoadTime.MostVisited", 1);
  histograms.ExpectTotalCount("NewTabPage.TilesReceivedTime", 1);
  histograms.ExpectTotalCount("NewTabPage.TilesReceivedTime.LocalNTP", 1);
  histograms.ExpectTotalCount("NewTabPage.TilesReceivedTime.MostVisited", 1);

  // Make sure impression metrics were recorded. There should be 2 tiles, the
  // default prepopulated TopSites (see history::PrepopulatedPage).
  histograms.ExpectTotalCount("NewTabPage.NumberOfTiles", 1);
  histograms.ExpectBucketCount("NewTabPage.NumberOfTiles", 2, 1);
  histograms.ExpectTotalCount("NewTabPage.SuggestionsImpression", 2);
  histograms.ExpectBucketCount("NewTabPage.SuggestionsImpression", 0, 1);
  histograms.ExpectBucketCount("NewTabPage.SuggestionsImpression", 1, 1);
  histograms.ExpectTotalCount("NewTabPage.SuggestionsImpression.client", 2);
  histograms.ExpectTotalCount("NewTabPage.SuggestionsImpression.Thumbnail", 2);
  histograms.ExpectTotalCount("NewTabPage.TileTitle", 2);
  histograms.ExpectTotalCount("NewTabPage.TileTitle.client", 2);
  histograms.ExpectTotalCount("NewTabPage.TileType", 2);
  histograms.ExpectTotalCount("NewTabPage.TileType.client", 2);
}

IN_PROC_BROWSER_TEST_F(LocalNTPTest, FrenchGoogleNTPLoadsWithoutError) {
  if (!local_ntp_test_utils::SwitchBrowserLanguageToFrench()) {
    LOG(ERROR) << "Failed switching to French language, aborting test.";
    return;
  }

  // Open a new blank tab.
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  ASSERT_FALSE(search::IsInstantNTP(active_tab));

  // Attach a console observer, listening for any message ("*" pattern).
  content::ConsoleObserverDelegate console_observer(active_tab, "*");
  active_tab->SetDelegate(&console_observer);

  // Navigate to the NTP.
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
  ASSERT_TRUE(search::IsInstantNTP(active_tab));
  ASSERT_EQ(GURL(chrome::kChromeSearchLocalNtpUrl),
            active_tab->GetController().GetVisibleEntry()->GetURL());
  // Make sure it's actually in French.
  ASSERT_EQ(base::ASCIIToUTF16("Nouvel onglet"), active_tab->GetTitle());

  // We shouldn't have gotten any console error messages.
  EXPECT_TRUE(console_observer.message().empty()) << console_observer.message();
}

// Tests that blink::UseCounter do not track feature usage for NTP activities.
IN_PROC_BROWSER_TEST_F(LocalNTPTest, ShouldNotTrackBlinkUseCounterForNTP) {
  base::HistogramTester histogram_tester;
  const char kFeaturesHistogramName[] = "Blink.UseCounter.Features";

  // Set up a test server, so we have some arbitrary non-NTP URL to navigate to.
  net::EmbeddedTestServer test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  test_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(test_server.Start());
  const GURL other_url = test_server.GetURL("/simple.html");

  // Open an NTP.
  content::WebContents* active_tab = local_ntp_test_utils::OpenNewTab(
      browser(), GURL(chrome::kChromeUINewTabURL));
  ASSERT_TRUE(search::IsInstantNTP(active_tab));
  // Expect no PageVisits count.
  EXPECT_EQ(nullptr,
            base::StatisticsRecorder::FindHistogram(kFeaturesHistogramName));
  // Navigate somewhere else in the same tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), other_url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  ASSERT_FALSE(search::IsInstantNTP(active_tab));
  // Navigate back to NTP.
  content::TestNavigationObserver back_observer(active_tab);
  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
  back_observer.Wait();
  ASSERT_TRUE(search::IsInstantNTP(active_tab));
  // There should be exactly 1 count of PageVisits.
  histogram_tester.ExpectBucketCount(
      kFeaturesHistogramName,
      static_cast<int32_t>(blink::mojom::WebFeature::kPageVisits), 1);

  // Navigate forward to the non-NTP page.
  content::TestNavigationObserver fwd_observer(active_tab);
  chrome::GoForward(browser(), WindowOpenDisposition::CURRENT_TAB);
  fwd_observer.Wait();
  ASSERT_FALSE(search::IsInstantNTP(active_tab));
  // Navigate to a new NTP instance.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL),
      WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  ASSERT_TRUE(search::IsInstantNTP(active_tab));
  // There should be 2 counts of PageVisits.
  histogram_tester.ExpectBucketCount(
      kFeaturesHistogramName,
      static_cast<int32_t>(blink::mojom::WebFeature::kPageVisits), 2);
}

class LocalNTPRTLTest : public LocalNTPTest {
 public:
  LocalNTPRTLTest() {}

 private:
  void SetUpCommandLine(base::CommandLine* cmdline) override {
    cmdline->AppendSwitchASCII(switches::kForceUIDirection,
                               switches::kForceDirectionRTL);
  }
};

IN_PROC_BROWSER_TEST_F(LocalNTPRTLTest, RightToLeft) {
  // Open an NTP.
  content::WebContents* active_tab = local_ntp_test_utils::OpenNewTab(
      browser(), GURL(chrome::kChromeUINewTabURL));
  ASSERT_TRUE(search::IsInstantNTP(active_tab));
  // Check that the "dir" attribute on the main "html" element says "rtl".
  std::string dir;
  ASSERT_TRUE(instant_test_utils::GetStringFromJS(
      active_tab, "document.documentElement.dir", &dir));
  EXPECT_EQ("rtl", dir);
}

namespace {

// Returns the RenderFrameHost corresponding to the most visited iframe in the
// given |tab|. |tab| must correspond to an NTP.
content::RenderFrameHost* GetMostVisitedIframe(content::WebContents* tab) {
  for (content::RenderFrameHost* frame : tab->GetAllFrames()) {
    if (frame->GetFrameName() == "mv-single") {
      return frame;
    }
  }
  return nullptr;
}

}  // namespace

IN_PROC_BROWSER_TEST_F(LocalNTPTest, LoadsIframe) {
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  NavigateToNTPAndWaitUntilLoaded();

  // Get the iframe and check that the tiles loaded correctly.
  content::RenderFrameHost* iframe = GetMostVisitedIframe(active_tab);

  // Get the total number of (non-empty) tiles from the iframe.
  int total_thumbs = 0;
  ASSERT_TRUE(instant_test_utils::GetIntFromJS(
      iframe, "document.querySelectorAll('.mv-thumb').length", &total_thumbs));
  // Also get how many of the tiles succeeded and failed in loading their
  // thumbnail images.
  int succeeded_imgs = 0;
  ASSERT_TRUE(instant_test_utils::GetIntFromJS(
      iframe, "document.querySelectorAll('.mv-thumb img').length",
      &succeeded_imgs));
  int failed_imgs = 0;
  ASSERT_TRUE(instant_test_utils::GetIntFromJS(
      iframe, "document.querySelectorAll('.mv-thumb.failed-img').length",
      &failed_imgs));

  // First, sanity check that the numbers line up (none of the css classes was
  // renamed, etc).
  EXPECT_EQ(total_thumbs, succeeded_imgs + failed_imgs);

  // Since we're in a non-signed-in, fresh profile with no history, there should
  // be the default TopSites tiles (see history::PrepopulatedPage).
  // Check that there is at least one tile, and that all of them loaded their
  // images successfully.
  EXPECT_GT(total_thumbs, 0);
  EXPECT_EQ(total_thumbs, succeeded_imgs);
  EXPECT_EQ(0, failed_imgs);
}
