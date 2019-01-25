// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/subprocess_metrics_provider.h"
#include "chrome/browser/page_load_metrics/observers/ads_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/use_counter_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_test_waiter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/subresource_filter/content/browser/ruleset_service.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/subresource_filter/core/common/activation_scope.h"
#include "components/subresource_filter/core/common/common_features.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/download/download_stats.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace {

const char kCrossOriginHistogramId[] =
    "PageLoad.Clients.Ads.SubresourceFilter.FrameCounts.AdFrames.PerFrame."
    "OriginStatus";

const char kAdUserActivationHistogramId[] =
    "PageLoad.Clients.Ads.SubresourceFilter.FrameCounts.AdFrames.PerFrame."
    "UserActivation";

enum class Origin {
  kNavigation,
  kAnchorAttribute,
};

std::ostream& operator<<(std::ostream& os, Origin origin) {
  switch (origin) {
    case Origin::kNavigation:
      return os << "Navigation";
    case Origin::kAnchorAttribute:
      return os << "AnchorAttribute";
  }
}

enum class SandboxOption {
  kNoSandbox,
  kDisallowDownloadsWithoutUserActivation,
  kAllowDownloadsWithoutUserActivation,
};

std::ostream& operator<<(std::ostream& os, SandboxOption sandbox_option) {
  switch (sandbox_option) {
    case SandboxOption::kNoSandbox:
      return os << "NoSandbox";
    case SandboxOption::kDisallowDownloadsWithoutUserActivation:
      return os << "DisallowDownloadsWithoutUserActivation";
    case SandboxOption::kAllowDownloadsWithoutUserActivation:
      return os << "AllowDownloadsWithoutUserActivation";
  }
}

// Allow PageLoadMetricsTestWaiter to be initialized for a new web content
// before the first commit.
class PopupPageLoadMetricsWaiterInitializer : public TabStripModelObserver {
 public:
  PopupPageLoadMetricsWaiterInitializer(
      TabStripModel* tab_strip_model,
      std::unique_ptr<page_load_metrics::PageLoadMetricsTestWaiter>* waiter)
      : waiter_(waiter), scoped_observer_(this) {
    scoped_observer_.Add(tab_strip_model);
  }

  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    if (change.type() == TabStripModelChange::kInserted &&
        selection.active_tab_changed()) {
      DCHECK(waiter_ && !(*waiter_));
      *waiter_ = std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
          tab_strip_model->GetActiveWebContents());
    }
  }

 private:
  std::unique_ptr<page_load_metrics::PageLoadMetricsTestWaiter>* waiter_ =
      nullptr;
  ScopedObserver<TabStripModel, PopupPageLoadMetricsWaiterInitializer>
      scoped_observer_;

  DISALLOW_COPY_AND_ASSIGN(PopupPageLoadMetricsWaiterInitializer);
};

}  // namespace

class AdsPageLoadMetricsObserverBrowserTest
    : public subresource_filter::SubresourceFilterBrowserTest {
 public:
  AdsPageLoadMetricsObserverBrowserTest()
      : subresource_filter::SubresourceFilterBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(subresource_filter::kAdTagging);
  }
  ~AdsPageLoadMetricsObserverBrowserTest() override {}

  std::unique_ptr<page_load_metrics::PageLoadMetricsTestWaiter>
  CreatePageLoadMetricsTestWaiter() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    return std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
        web_contents);
  }

  void SetUpOnMainThread() override {
    SubresourceFilterBrowserTest::SetUpOnMainThread();
    SetRulesetWithRules(
        {subresource_filter::testing::CreateSuffixRule("ad_iframe_writer.js"),
         subresource_filter::testing::CreateSuffixRule("ad_script.js")});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(AdsPageLoadMetricsObserverBrowserTest);
};

// Test that an embedded ad is same origin.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       OriginStatusMetricEmbedded) {
  base::HistogramTester histogram_tester;
  auto waiter = CreatePageLoadMetricsTestWaiter();
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/ads_observer/srcdoc_embedded_ad.html"));
  waiter->AddMinimumCompleteResourcesExpectation(4);
  waiter->Wait();
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  histogram_tester.ExpectUniqueSample(
      kCrossOriginHistogramId,
      AdsPageLoadMetricsObserver::AdOriginStatus::kSame, 1);
}

// Test that an empty embedded ad isn't reported at all.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       OriginStatusMetricEmbeddedEmpty) {
  base::HistogramTester histogram_tester;
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/ads_observer/srcdoc_embedded_ad_empty.html"));
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  histogram_tester.ExpectTotalCount(kCrossOriginHistogramId, 0);
}

// Test that an ad with the same origin as the main page is same origin.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       OriginStatusMetricSame) {
  base::HistogramTester histogram_tester;
  auto waiter = CreatePageLoadMetricsTestWaiter();
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/ads_observer/same_origin_ad.html"));
  waiter->AddMinimumCompleteResourcesExpectation(4);
  waiter->Wait();

  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  histogram_tester.ExpectUniqueSample(
      kCrossOriginHistogramId,
      AdsPageLoadMetricsObserver::AdOriginStatus::kSame, 1);
}

// Test that an ad with a different origin as the main page is cross origin.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       OriginStatusMetricCross) {
  // Note: Cannot navigate cross-origin without dynamically generating the URL.
  base::HistogramTester histogram_tester;
  auto waiter = CreatePageLoadMetricsTestWaiter();

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/iframe_blank.html"));
  // Note that the initial iframe is not an ad, so the metric doesn't observe
  // it initially as same origin.  However, on re-navigating to a cross
  // origin site that has an ad at its origin, the ad on that page is cross
  // origin from the original page.
  NavigateIframeToURL(web_contents(), "test",
                      embedded_test_server()->GetURL(
                          "a.com", "/ads_observer/same_origin_ad.html"));

  // Wait until all resource data updates are sent.
  waiter->AddPageExpectation(
      page_load_metrics::PageLoadMetricsTestWaiter::TimingField::kLoadEvent);
  waiter->Wait();
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  histogram_tester.ExpectUniqueSample(
      kCrossOriginHistogramId,
      AdsPageLoadMetricsObserver::AdOriginStatus::kCross, 1);
}

IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       UserActivationSetOnFrame) {
  base::HistogramTester histogram_tester;
  auto waiter = CreatePageLoadMetricsTestWaiter();
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "foo.com", "/ad_tagging/frame_factory.html"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Create a second frame that will not receive activation.
  EXPECT_TRUE(content::ExecuteScriptWithoutUserGesture(
      web_contents, "createAdFrame('/ad_tagging/frame_factory.html', '');"));
  EXPECT_TRUE(content::ExecuteScriptWithoutUserGesture(
      web_contents, "createAdFrame('/ad_tagging/frame_factory.html', '');"));

  // Wait for the frames resources to be loaded as we only log histograms for
  // frames that have non-zero bytes. Four resources per frame and one favicon.
  waiter->AddMinimumCompleteResourcesExpectation(13);
  waiter->Wait();

  // Activate one frame by executing a dummy script.
  content::RenderFrameHost* ad_frame =
      ChildFrameAt(web_contents->GetMainFrame(), 0);
  const std::string no_op_script = "// No-op script";
  EXPECT_TRUE(ExecuteScript(ad_frame, no_op_script));

  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  histogram_tester.ExpectBucketCount(
      kAdUserActivationHistogramId,
      AdsPageLoadMetricsObserver::UserActivationStatus::kReceivedActivation, 1);
  histogram_tester.ExpectBucketCount(
      kAdUserActivationHistogramId,
      AdsPageLoadMetricsObserver::UserActivationStatus::kNoActivation, 1);
}

// Test that a subframe that aborts (due to doc.write) doesn't cause a crash
// if it continues to load resources.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       DocOverwritesNavigation) {
  content::DOMMessageQueue msg_queue;

  base::HistogramTester histogram_tester;

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/ads_observer/docwrite_provisional_frame.html"));
  std::string status;
  EXPECT_TRUE(msg_queue.WaitForMessage(&status));
  EXPECT_EQ("\"loaded\"", status);

  // Navigate away to force the histogram recording.
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));

  // TODO(johnidel): Check that the subresources of the new frame are reported
  // correctly. Resources from a failed provisional load are not reported to
  // resource data updates, causing this adframe to not be recorded. This is an
  // uncommon case but should be reported. See crbug.com/914893.
}

// Test that a blank ad subframe that is docwritten correctly reports metrics.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       DocWriteAboutBlankAdframe) {
  base::HistogramTester histogram_tester;
  auto waiter = CreatePageLoadMetricsTestWaiter();
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL(
                                   "/ads_observer/docwrite_blank_frame.html"));
  waiter->AddMinimumCompleteResourcesExpectation(5);
  waiter->Wait();
  // Navigate away to force the histogram recording.
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  histogram_tester.ExpectUniqueSample(
      "PageLoad.Clients.Ads.SubresourceFilter.FrameCounts.AnyParentFrame."
      "AdFrames",
      1, 1);
  histogram_tester.ExpectUniqueSample(
      "PageLoad.Clients.Ads.Bytes.AdFrames.Aggregate.Total", 0 /* < 1 KB */, 1);
}

IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       SubresourceFilter) {
  ResetConfiguration(subresource_filter::Configuration(
      subresource_filter::mojom::ActivationLevel::kDryRun,
      subresource_filter::ActivationScope::ALL_SITES));
  base::HistogramTester histogram_tester;

  // cross_site_iframe_factory loads URLs like:
  // http://b.com:40919/cross_site_iframe_factory.html?b()
  SetRulesetToDisallowURLsWithPathSuffix("b()");
  const GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b,b,c,d)"));

  auto waiter = CreatePageLoadMetricsTestWaiter();
  ui_test_utils::NavigateToURL(browser(), main_url);

  // One favicon resource and 2 resources for each frame.
  waiter->AddMinimumCompleteResourcesExpectation(11);
  waiter->Wait();

  // Navigate away to force the histogram recording.
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));

  histogram_tester.ExpectUniqueSample(
      "PageLoad.Clients.Ads.SubresourceFilter.FrameCounts.AnyParentFrame."
      "AdFrames",
      2, 1);
}

class AdsPageLoadMetricsTestWaiter
    : public page_load_metrics::PageLoadMetricsTestWaiter {
 public:
  explicit AdsPageLoadMetricsTestWaiter(content::WebContents* web_contents)
      : page_load_metrics::PageLoadMetricsTestWaiter(web_contents) {}
  void AddMinimumAdResourceExpectation(int num_ad_resources) {
    expected_minimum_num_ad_resources_ = num_ad_resources;
  }

 protected:
  bool ExpectationsSatisfied() const override {
    int num_ad_resources = 0;
    for (auto& kv : page_resources_) {
      if (kv.second->reported_as_ad_resource)
        num_ad_resources++;
    }
    return num_ad_resources >= expected_minimum_num_ad_resources_ &&
           PageLoadMetricsTestWaiter::ExpectationsSatisfied();
  };

 private:
  int expected_minimum_num_ad_resources_ = 0;
};

class AdsPageLoadMetricsObserverResourceBrowserTest
    : public subresource_filter::SubresourceFilterBrowserTest {
 public:
  AdsPageLoadMetricsObserverResourceBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(subresource_filter::kAdTagging);
  }

  ~AdsPageLoadMetricsObserverResourceBrowserTest() override {}
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    SetRulesetWithRules(
        {subresource_filter::testing::CreateSuffixRule("ad_script.js"),
         subresource_filter::testing::CreateSuffixRule("ad_script_2.js"),
         subresource_filter::testing::CreateSuffixRule("disallow.zip")});
  }

  void OpenLinkInFrame(const content::ToRenderFrameHost& adapter,
                       const std::string& link_id,
                       bool has_gesture) {
    std::string open_link_script = base::StringPrintf(
        R"(
            var evt = document.createEvent("MouseEvent");
            evt.initMouseEvent('click', true, true);
            document.getElementById('%s').dispatchEvent(evt);
        )",
        link_id.c_str());
    if (has_gesture) {
      EXPECT_TRUE(ExecuteScript(adapter, open_link_script));
    } else {
      EXPECT_TRUE(ExecuteScriptWithoutUserGesture(adapter, open_link_script));
    }
  }

 protected:
  std::unique_ptr<AdsPageLoadMetricsTestWaiter>
  CreateAdsPageLoadMetricsTestWaiter() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    return std::make_unique<AdsPageLoadMetricsTestWaiter>(web_contents);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverResourceBrowserTest,
                       ReceivedAdResources) {
  embedded_test_server()->ServeFilesFromSourceDirectory(
      "chrome/test/data/ad_tagging");
  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreateAdsPageLoadMetricsTestWaiter();
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("foo.com", "/frame_factory.html"));
  // Two subresources should have been reported as ads.
  waiter->AddMinimumAdResourceExpectation(2);
  waiter->Wait();
}

// Main resources for adframes are counted as ad resources.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverResourceBrowserTest,
                       ReceivedMainResourceAds) {
  embedded_test_server()->ServeFilesFromSourceDirectory(
      "chrome/test/data/ad_tagging");
  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreateAdsPageLoadMetricsTestWaiter();

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("foo.com", "/frame_factory.html"));
  contents->GetMainFrame()->ExecuteJavaScriptForTests(
      base::ASCIIToUTF16("createAdFrame('frame_factory.html', '');"));
  // Two pages subresources should have been reported as ad. The iframe resource
  // and its three subresources should also be reported as ads.
  waiter->AddMinimumAdResourceExpectation(6);
  waiter->Wait();
}

// Subframe navigations report ad resources correctly.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverResourceBrowserTest,
                       ReceivedSubframeNavigationAds) {
  embedded_test_server()->ServeFilesFromSourceDirectory(
      "chrome/test/data/ad_tagging");
  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreateAdsPageLoadMetricsTestWaiter();

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("foo.com", "/frame_factory.html"));
  contents->GetMainFrame()->ExecuteJavaScriptForTests(
      base::ASCIIToUTF16("createAdFrame('frame_factory.html', 'test');"));
  waiter->AddMinimumAdResourceExpectation(6);
  waiter->Wait();
  NavigateIframeToURL(
      web_contents(), "test",
      embedded_test_server()->GetURL("foo.com", "/frame_factory.html"));
  // The new subframe and its three subresources should be reported
  // as ads.
  waiter->AddMinimumAdResourceExpectation(10);
  waiter->Wait();
}

// Verify that per-resource metrics are recorded correctly.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverResourceBrowserTest,
                       ReceivedAdResourceMetrics) {
  base::HistogramTester histogram_tester;

  const char kHttpResponseHeader[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "\r\n";
  auto main_html_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          embedded_test_server(), "/mock_page.html",
          true /*relative_url_is_prefix*/);
  auto ad_script_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          embedded_test_server(), "/ad_script.js",
          true /*relative_url_is_prefix*/);
  auto iframe_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          embedded_test_server(), "/iframe.html",
          true /*relative_url_is_prefix*/);
  auto vanilla_script_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          embedded_test_server(), "/vanilla_script.js",
          true /*relative_url_is_prefix*/);
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreateAdsPageLoadMetricsTestWaiter();

  browser()->OpenURL(content::OpenURLParams(
      embedded_test_server()->GetURL("/mock_page.html"), content::Referrer(),
      WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_TYPED, false));

  main_html_response->WaitForRequest();
  main_html_response->Send(kHttpResponseHeader);
  main_html_response->Send(
      "<html><body></body><script src=\"ad_script.js\"></script></html>");
  main_html_response->Done();

  ad_script_response->WaitForRequest();
  ad_script_response->Send(kHttpResponseHeader);
  ad_script_response->Send(
      "var iframe = document.createElement(\"iframe\");"
      "iframe.src =\"iframe.html\";"
      "document.body.appendChild(iframe);");
  ad_script_response->Send(std::string(1000, ' '));
  ad_script_response->Done();

  iframe_response->WaitForRequest();
  iframe_response->Send(kHttpResponseHeader);
  iframe_response->Send("<html><script src=\"vanilla_script.js\"></script>");
  iframe_response->Send(std::string(2000, ' '));
  iframe_response->Send("</html>");
  iframe_response->Done();

  vanilla_script_response->WaitForRequest();
  vanilla_script_response->Send(kHttpResponseHeader);
  vanilla_script_response->Send(std::string(1024, ' '));
  waiter->AddMinimumNetworkBytesExpectation(4000);
  waiter->Wait();

  // Verify correct numbers of resources are recorded.
  histogram_tester.ExpectTotalCount(
      "Ads.ResourceUsage.Size.Network.Mainframe.VanillaResource", 1);
  histogram_tester.ExpectTotalCount(
      "Ads.ResourceUsage.Size.Network.Mainframe.AdResource", 1);
  histogram_tester.ExpectTotalCount(
      "Ads.ResourceUsage.Size.Network.Subframe.AdResource", 1);
  // Verify unfinished resource not yet recorded.
  histogram_tester.ExpectTotalCount(
      "Ads.ResourceUsage.Size.Network.Subframe.VanillaResource", 0);

  // Close all tabs instead of navigating as the embedded_test_server will
  // hang waiting for loads to finish when we have an unfinished
  // ControlledHttpReseonse.
  browser()->tab_strip_model()->CloseAllTabs();

  // Verify unfinished resource recorded when page is destroyed.
  histogram_tester.ExpectTotalCount(
      "Ads.ResourceUsage.Size.Network.Subframe.AdResource", 2);

  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.Ads.Bytes.FullPage.Network", 4, 1);
  // We have received 4 KB of ads and 1 KB of toplevel ads.
  histogram_tester.ExpectBucketCount("PageLoad.Clients.Ads.Resources.Bytes.Ads",
                                     4, 1);
  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.Ads.Resources.Bytes.TopLevelAds", 1, 1);

  // 4 resources loaded, one unfinished.
  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.Ads.Resources.Bytes.Unfinished", 1, 1);
}

IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverResourceBrowserTest,
                       IncompleteResourcesRecordedToFrameMetrics) {
  base::HistogramTester histogram_tester;
  SetRulesetWithRules(
      {subresource_filter::testing::CreateSuffixRule("ad_iframe_writer.js")});
  embedded_test_server()->ServeFilesFromSourceDirectory(
      "chrome/test/data/ads_observer");
  content::SetupCrossSiteRedirector(embedded_test_server());

  const char kHttpResponseHeader[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "\r\n";
  auto incomplete_resource_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          embedded_test_server(), "/incomplete_resource.js",
          true /*relative_url_is_prefix*/);
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreateAdsPageLoadMetricsTestWaiter();

  browser()->OpenURL(content::OpenURLParams(
      embedded_test_server()->GetURL("/ad_with_incomplete_resource.html"),
      content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
      ui::PAGE_TRANSITION_TYPED, false));

  waiter->AddMinimumCompleteResourcesExpectation(3);
  waiter->Wait();
  int64_t initial_page_bytes = waiter->current_network_bytes();

  // Ad resource will not finish loading but should be reported to metrics.
  incomplete_resource_response->WaitForRequest();
  incomplete_resource_response->Send(kHttpResponseHeader);
  incomplete_resource_response->Send(std::string(2048, ' '));

  // Wait for the resource update to be received for the incomplete response.
  waiter->AddMinimumNetworkBytesExpectation(2048);
  waiter->Wait();

  // Close all tabs instead of navigating as the embedded_test_server will
  // hang waiting for loads to finish when we have an unfinished
  // ControlledHttpResponse.
  browser()->tab_strip_model()->CloseAllTabs();

  int expected_page_kilobytes = (initial_page_bytes + 2048) / 1024;

  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.Ads.Bytes.FullPage.Network", expected_page_kilobytes,
      1);
  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.Ads.Bytes.AdFrames.Aggregate.Network", 2, 1);
  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.Ads.Bytes.AdFrames.Aggregate.Total", 2, 1);
  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.Ads.Bytes.AdFrames.PerFrame.Network", 2, 1);
  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.Ads.Bytes.AdFrames.PerFrame.Total", 2, 1);
}

// Verify that per-resource metrics are reported for cached resources and
// resources loaded by the network.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverResourceBrowserTest,
                       RecordedCacheResourceMetrics) {
  base::HistogramTester histogram_tester;
  SetRulesetWithRules(
      {subresource_filter::testing::CreateSuffixRule("create_frame.js")});
  embedded_test_server()->ServeFilesFromSourceDirectory(
      "chrome/test/data/ad_tagging");
  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreateAdsPageLoadMetricsTestWaiter();
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("foo.com", "/cachetime"));

  // Wait for the favicon to be fetched.
  waiter->AddMinimumCompleteResourcesExpectation(2);
  waiter->Wait();

  // All resources should have been loaded by network.
  histogram_tester.ExpectTotalCount(
      "Ads.ResourceUsage.Size.Network.Mainframe.VanillaResource", 2);

  // Open a new tab and navigate so that resources are fetched via the disk
  // cache. Navigating to the same URL in the same tab triggers a refresh which
  // will not check the disk cache.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
          ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  waiter = CreateAdsPageLoadMetricsTestWaiter();
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("foo.com", "/cachetime"));

  // Wait for the resource to be fetched.
  waiter->AddMinimumCompleteResourcesExpectation(1);
  waiter->Wait();

  // Resource should be recorded as loaded from the cache. Favicon not
  // fetched this time.
  histogram_tester.ExpectTotalCount(
      "Ads.ResourceUsage.Size.Cache.Mainframe.VanillaResource", 1);
}

// Verify that Mime type metrics are recorded correctly.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverResourceBrowserTest,
                       RecordedMimeMetrics) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  embedded_test_server()->ServeFilesFromSourceDirectory(
      "chrome/test/data/ad_tagging");
  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreateAdsPageLoadMetricsTestWaiter();

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL url = embedded_test_server()->GetURL("foo.com", "/frame_factory.html");
  ui_test_utils::NavigateToURL(browser(), url);
  contents->GetMainFrame()->ExecuteJavaScriptForTests(
      base::ASCIIToUTF16("createAdFrame('multiple_mimes.html', 'test');"));
  waiter->AddMinimumAdResourceExpectation(8);
  waiter->Wait();

  // Close all tabs to log metrics, as the video resource request is incomplete.
  browser()->tab_strip_model()->CloseAllTabs();

  histogram_tester.ExpectTotalCount("Ads.ResourceUsage.Size.Network.Mime.HTML",
                                    1);
  histogram_tester.ExpectTotalCount("Ads.ResourceUsage.Size.Network.Mime.CSS",
                                    1);
  histogram_tester.ExpectTotalCount("Ads.ResourceUsage.Size.Network.Mime.JS",
                                    3);

  // Note: png and video/webm mime types are not set explicitly by the
  // embedded_test_server.
  histogram_tester.ExpectTotalCount("Ads.ResourceUsage.Size.Network.Mime.Image",
                                    1);
  histogram_tester.ExpectTotalCount("Ads.ResourceUsage.Size.Network.Mime.Video",
                                    1);
  histogram_tester.ExpectTotalCount("Ads.ResourceUsage.Size.Network.Mime.Other",
                                    1);

  // Verify UKM Metrics recorded.
  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::AdPageLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntrySourceHasUrl(entries.front(), url);
  EXPECT_GT(*ukm_recorder.GetEntryMetric(
                entries.front(), ukm::builders::AdPageLoad::kAdBytesName),
            0);
  EXPECT_GT(
      *ukm_recorder.GetEntryMetric(
          entries.front(), ukm::builders::AdPageLoad::kAdBytesPerSecondName),
      0);

  // TTI is not reached by this page and thus should not have this recorded.
  EXPECT_FALSE(ukm_recorder.EntryHasMetric(
      entries.front(),
      ukm::builders::AdPageLoad::kAdBytesPerSecondAfterInteractiveName));
  EXPECT_GT(
      *ukm_recorder.GetEntryMetric(
          entries.front(), ukm::builders::AdPageLoad::kAdJavascriptBytesName),
      0);
  EXPECT_GT(*ukm_recorder.GetEntryMetric(
                entries.front(), ukm::builders::AdPageLoad::kAdVideoBytesName),
            0);
}

// Download gets blocked when LoadPolicy is DISALLOW for the navigation
// to download.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverResourceBrowserTest,
                       SubframeNavigationDownloadBlockedByLoadPolicy) {
  ResetConfiguration(subresource_filter::Configuration(
      subresource_filter::mojom::ActivationLevel::kEnabled,
      subresource_filter::ActivationScope::ALL_SITES));

  base::HistogramTester histogram_tester;

  embedded_test_server()->ServeFilesFromSourceDirectory(
      "chrome/test/data/ad_tagging");
  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  std::string host_name = "foo.com";
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(host_name, "/frame_factory.html"));
  content::TestNavigationObserver navigation_observer(web_contents());
  contents->GetMainFrame()->ExecuteJavaScriptForTests(
      base::ASCIIToUTF16("createFrame('download.html', 'test');"));
  navigation_observer.Wait();

  content::RenderFrameHost* rfh = content::FrameMatchingPredicate(
      web_contents(), base::BindRepeating(&content::FrameMatchesName, "test"));
  OpenLinkInFrame(rfh, "blocked_nav_download_id", false /* gesture*/);

  SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histogram_tester.ExpectTotalCount("Download.Subframe.SandboxOriginAdGesture",
                                    0);
}

class RemoteFrameNavigationBrowserTest
    : public AdsPageLoadMetricsObserverResourceBrowserTest {
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        "enable-blink-features",
        "BlockingDownloadsInSandboxWithoutUserActivation");
  }
};

IN_PROC_BROWSER_TEST_F(RemoteFrameNavigationBrowserTest,
                       DownloadsBlockedInSandbox) {
  embedded_test_server()->ServeFilesFromSourceDirectory(
      "chrome/test/data/ad_tagging");
  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  base::HistogramTester histogram_tester;
  std::string origin1 = "foo.com";
  std::string origin2 = "bar.com";
  GURL tab1_url =
      embedded_test_server()->GetURL(origin1, "/frame_factory.html");

  auto subframe_navigation_waiter =
      std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
          web_contents());
  subframe_navigation_waiter->AddSubframeNavigationExpectation(2);

  ui_test_utils::NavigateToURL(browser(), tab1_url);

  std::string subframe_url =
      embedded_test_server()->GetURL(origin2, "/frame_factory.html").spec();
  content::TestNavigationObserver new_subframe_waiter(web_contents());
  std::string script =
      base::StringPrintf("createFrame('%s','test','');", subframe_url.c_str());
  web_contents()->GetMainFrame()->ExecuteJavaScriptForTests(
      base::ASCIIToUTF16(script));
  new_subframe_waiter.Wait();

  GURL dld_url = embedded_test_server()->GetURL(origin1, "/allow.zip");
  EXPECT_TRUE(ExecuteScriptWithoutUserGesture(
      web_contents(),
      "document.getElementById('test').src = \"" + dld_url.spec() + "\";"));

  subframe_navigation_waiter->Wait();
  SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histogram_tester.ExpectTotalCount("Download.Subframe.SandboxOriginAdGesture",
                                    0 /* expected_count */);
}

class MainFrameDownloadFlagsBrowserTest
    : public AdsPageLoadMetricsObserverResourceBrowserTest,
      public ::testing::WithParamInterface<std::tuple<
          Origin,
          bool /* enable_blocking_downloads_in_sandbox_without_user_activation
                */
          ,
          SandboxOption,
          bool /* has_gesture */>> {
  void SetUpCommandLine(base::CommandLine* command_line) override {
    bool enable_blocking_downloads_in_sandbox_without_user_activation;
    std::tie(std::ignore,
             enable_blocking_downloads_in_sandbox_without_user_activation,
             std::ignore, std::ignore) = GetParam();
    std::string cmd =
        enable_blocking_downloads_in_sandbox_without_user_activation
            ? "enable-blink-features"
            : "disable-blink-features";
    command_line->AppendSwitchASCII(
        cmd, "BlockingDownloadsInSandboxWithoutUserActivation");
  }
};

// Main frame download events are reported correctly.
IN_PROC_BROWSER_TEST_P(MainFrameDownloadFlagsBrowserTest, Download) {
  Origin origin;
  bool enable_blocking_downloads_in_sandbox_without_user_activation;
  SandboxOption sandbox_option;
  bool has_gesture;
  std::tie(origin, enable_blocking_downloads_in_sandbox_without_user_activation,
           sandbox_option, has_gesture) = GetParam();
  SCOPED_TRACE(
      ::testing::Message()
      << "origin = " << origin << ", "
      << "enable_blocking_downloads_in_sandbox_without_user_activation = "
      << enable_blocking_downloads_in_sandbox_without_user_activation << ", "
      << "sandbox_option = " << sandbox_option << ", "
      << "has_gesture = " << has_gesture);

  bool expected_download =
      !enable_blocking_downloads_in_sandbox_without_user_activation ||
      has_gesture ||
      sandbox_option != SandboxOption::kDisallowDownloadsWithoutUserActivation;
  bool expected_sandbox_bit =
      enable_blocking_downloads_in_sandbox_without_user_activation
          ? has_gesture &&
                sandbox_option ==
                    SandboxOption::kDisallowDownloadsWithoutUserActivation
          : sandbox_option != SandboxOption::kNoSandbox;

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  embedded_test_server()->ServeFilesFromSourceDirectory(
      "chrome/test/data/ad_tagging");
  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  std::string host_name = "foo.com";
  GURL main_url = embedded_test_server()->GetURL(host_name, "/download.html");

  std::unique_ptr<page_load_metrics::PageLoadMetricsTestWaiter>
      web_feature_waiter;

  if (sandbox_option == SandboxOption::kNoSandbox) {
    ui_test_utils::NavigateToURL(browser(), main_url);
  } else {
    GURL first_tab_url =
        embedded_test_server()->GetURL(host_name, "/frame_factory.html");
    ui_test_utils::NavigateToURL(browser(), first_tab_url);
    const char* method = "createFrame";
    std::string subframe_url =
        embedded_test_server()->GetURL(host_name, "/frame_factory.html").spec();
    const char* id = "test";
    const char* sandbox_param =
        sandbox_option == SandboxOption::kDisallowDownloadsWithoutUserActivation
            ? "'allow-scripts allow-same-origin allow-popups'"
            : "'allow-scripts allow-same-origin allow-popups "
              "allow-downloads-without-user-activation'";
    content::TestNavigationObserver navigation_observer(web_contents());
    std::string script = base::StringPrintf(
        "%s('%s','%s',%s);", method, subframe_url.c_str(), id, sandbox_param);
    web_contents()->GetMainFrame()->ExecuteJavaScriptForTests(
        base::ASCIIToUTF16(script));
    navigation_observer.Wait();

    content::RenderFrameHost* child = content::FrameMatchingPredicate(
        web_contents(), base::BindRepeating(&content::FrameMatchesName, id));

    std::unique_ptr<PopupPageLoadMetricsWaiterInitializer> waiter_initializer;
    if (expected_sandbox_bit) {
      waiter_initializer =
          std::make_unique<PopupPageLoadMetricsWaiterInitializer>(
              browser()->tab_strip_model(), &web_feature_waiter);
    }
    content::TestNavigationObserver popup_observer(main_url);
    popup_observer.StartWatchingNewWebContents();
    EXPECT_TRUE(
        ExecuteScript(child, "window.open(\"" + main_url.spec() + "\");"));
    popup_observer.Wait();
    ASSERT_EQ(2, browser()->tab_strip_model()->count());
  }

  DCHECK(!expected_sandbox_bit || web_feature_waiter);
  if (expected_sandbox_bit) {
    blink::mojom::WebFeature feature =
        origin == Origin::kNavigation
            ? has_gesture ? blink::mojom::WebFeature::
                                kNavigationDownloadInSandboxWithUserGesture
                          : blink::mojom::WebFeature::
                                kNavigationDownloadInSandboxWithoutUserGesture
            : has_gesture
                  ? blink::mojom::WebFeature::
                        kHTMLAnchorElementDownloadInSandboxWithUserGesture
                  : blink::mojom::WebFeature::
                        kHTMLAnchorElementDownloadInSandboxWithoutUserGesture;
    web_feature_waiter->AddWebFeatureExpectation(feature);
  }

  std::string link_id =
      origin == Origin::kNavigation ? "nav_download_id" : "anchor_download_id";

  std::unique_ptr<content::DownloadTestObserver> download_observer(
      new content::DownloadTestObserverTerminal(
          content::BrowserContext::GetDownloadManager(browser()->profile()),
          expected_download /* wait_count */,
          content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL));
  OpenLinkInFrame(web_contents(), link_id, has_gesture);
  download_observer->WaitForFinished();
  if (web_feature_waiter)
    web_feature_waiter->Wait();
  SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  if (!expected_download) {
    histogram_tester.ExpectTotalCount("Download.MainFrame.SandboxGesture",
                                      0 /* expected_count */);
    return;
  }

  blink::DownloadStats::MainFrameDownloadFlags expected_flags;
  expected_flags.has_sandbox = expected_sandbox_bit;
  expected_flags.has_gesture = has_gesture;
  histogram_tester.ExpectUniqueSample("Download.MainFrame.SandboxGesture",
                                      expected_flags.ToUmaValue(),
                                      1 /* expected_count */);

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::MainFrameDownload::kEntryName);
  EXPECT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntrySourceHasUrl(entries.back(), main_url);
  ukm_recorder.ExpectEntryMetric(
      entries.back(), ukm::builders::MainFrameDownload::kHasSandboxName,
      expected_sandbox_bit);
  ukm_recorder.ExpectEntryMetric(
      entries.back(), ukm::builders::MainFrameDownload::kHasGestureName,
      has_gesture);
}

INSTANTIATE_TEST_CASE_P(
    /* no prefix */,
    MainFrameDownloadFlagsBrowserTest,
    ::testing::Combine(
        ::testing::Values(Origin::kNavigation, Origin::kAnchorAttribute),
        ::testing::Bool(),
        ::testing::Values(
            SandboxOption::kNoSandbox,
            SandboxOption::kDisallowDownloadsWithoutUserActivation,
            SandboxOption::kAllowDownloadsWithoutUserActivation),
        ::testing::Bool()));

class SubframeDownloadFlagsBrowserTest
    : public AdsPageLoadMetricsObserverResourceBrowserTest,
      public ::testing::WithParamInterface<std::tuple<
          Origin,
          bool /* enable_blocking_downloads_in_sandbox_without_user_activation
                */
          ,
          SandboxOption,
          bool /* is_cross_origin */,
          bool /* is_ad_frame */,
          bool /* has_gesture */>> {
  void SetUpCommandLine(base::CommandLine* command_line) override {
    bool enable_blocking_downloads_in_sandbox_without_user_activation;
    std::tie(std::ignore,
             enable_blocking_downloads_in_sandbox_without_user_activation,
             std::ignore, std::ignore, std::ignore, std::ignore) = GetParam();
    std::string cmd =
        enable_blocking_downloads_in_sandbox_without_user_activation
            ? "enable-blink-features"
            : "disable-blink-features";
    command_line->AppendSwitchASCII(
        cmd, "BlockingDownloadsInSandboxWithoutUserActivation");
  }
};

// Subframe download events are reported correctly.
IN_PROC_BROWSER_TEST_P(SubframeDownloadFlagsBrowserTest, Download) {
  Origin origin;
  bool enable_blocking_downloads_in_sandbox_without_user_activation;
  SandboxOption sandbox_option;
  bool is_cross_origin;
  bool is_ad_frame;
  bool has_gesture;
  std::tie(origin, enable_blocking_downloads_in_sandbox_without_user_activation,
           sandbox_option, is_cross_origin, is_ad_frame, has_gesture) =
      GetParam();
  SCOPED_TRACE(
      ::testing::Message()
      << "origin = " << origin << ", "
      << "enable_blocking_downloads_in_sandbox_without_user_activation = "
      << enable_blocking_downloads_in_sandbox_without_user_activation << ", "
      << "sandbox_option = " << sandbox_option << ", "
      << "is_cross_origin = " << is_cross_origin << ", "
      << "is_ad_frame = " << is_ad_frame << ", "
      << "has_gesture = " << has_gesture);

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  bool expected_download =
      !enable_blocking_downloads_in_sandbox_without_user_activation ||
      has_gesture ||
      sandbox_option != SandboxOption::kDisallowDownloadsWithoutUserActivation;
  bool expected_sandbox_bit =
      enable_blocking_downloads_in_sandbox_without_user_activation
          ? has_gesture &&
                sandbox_option ==
                    SandboxOption::kDisallowDownloadsWithoutUserActivation
          : sandbox_option != SandboxOption::kNoSandbox;

  std::unique_ptr<content::DownloadTestObserver> download_observer(
      new content::DownloadTestObserverTerminal(
          content::BrowserContext::GetDownloadManager(browser()->profile()),
          expected_download /* wait_count */,
          content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL));

  embedded_test_server()->ServeFilesFromSourceDirectory(
      "chrome/test/data/ad_tagging");
  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  std::unique_ptr<AdsPageLoadMetricsTestWaiter> waiter;
  if (origin == Origin::kNavigation) {
    waiter = std::make_unique<AdsPageLoadMetricsTestWaiter>(contents);
    waiter->AddSubframeNavigationExpectation(2);
  }
  if (expected_download && is_ad_frame) {
    if (!waiter)
      waiter = std::make_unique<AdsPageLoadMetricsTestWaiter>(contents);
    blink::mojom::WebFeature feature =
        has_gesture
            ? blink::mojom::WebFeature::kDownloadInAdFrameWithUserGesture
            : blink::mojom::WebFeature::kDownloadInAdFrameWithoutUserGesture;
    waiter->AddWebFeatureExpectation(feature);
  }
  if (expected_sandbox_bit) {
    if (!waiter)
      waiter = std::make_unique<AdsPageLoadMetricsTestWaiter>(contents);
    blink::mojom::WebFeature feature =
        origin == Origin::kNavigation
            ? has_gesture ? blink::mojom::WebFeature::
                                kNavigationDownloadInSandboxWithUserGesture
                          : blink::mojom::WebFeature::
                                kNavigationDownloadInSandboxWithoutUserGesture
            : has_gesture
                  ? blink::mojom::WebFeature::
                        kHTMLAnchorElementDownloadInSandboxWithUserGesture
                  : blink::mojom::WebFeature::
                        kHTMLAnchorElementDownloadInSandboxWithoutUserGesture;
    waiter->AddWebFeatureExpectation(feature);
  }

  std::string host_name = "foo.com";
  GURL main_url =
      embedded_test_server()->GetURL(host_name, "/frame_factory.html");
  ui_test_utils::NavigateToURL(browser(), main_url);

  std::string link_id =
      origin == Origin::kNavigation ? "nav_download_id" : "anchor_download_id";

  const char* method = is_ad_frame ? "createAdFrame" : "createFrame";
  std::string url =
      embedded_test_server()
          ->GetURL(is_cross_origin ? "bar.com" : host_name, "/download.html")
          .spec();
  const char* id = "test";
  const char* sandbox_param =
      sandbox_option == SandboxOption::kNoSandbox
          ? "undefined"
          : sandbox_option ==
                    SandboxOption::kDisallowDownloadsWithoutUserActivation
                ? "'allow-scripts allow-same-origin'"
                : "'allow-scripts allow-same-origin "
                  "allow-downloads-without-user-activation'";

  content::TestNavigationObserver navigation_observer(web_contents());
  std::string script = base::StringPrintf("%s('%s','%s',%s);", method,
                                          url.c_str(), id, sandbox_param);

  contents->GetMainFrame()->ExecuteJavaScriptForTests(
      base::ASCIIToUTF16(script));
  navigation_observer.Wait();

  content::RenderFrameHost* rfh = content::FrameMatchingPredicate(
      web_contents(), base::BindRepeating(&content::FrameMatchesName, id));
  OpenLinkInFrame(rfh, link_id, has_gesture);

  download_observer->WaitForFinished();
  if (waiter)
    waiter->Wait();
  SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  if (!expected_download) {
    histogram_tester.ExpectTotalCount(
        "Download.Subframe.SandboxOriginAdGesture", 0 /* expected_count */);
    return;
  }

  blink::DownloadStats::SubframeDownloadFlags expected_flags;
  expected_flags.has_sandbox = expected_sandbox_bit;
  expected_flags.is_cross_origin = is_cross_origin;
  expected_flags.is_ad_frame = is_ad_frame;
  expected_flags.has_gesture = has_gesture;
  histogram_tester.ExpectUniqueSample(
      "Download.Subframe.SandboxOriginAdGesture", expected_flags.ToUmaValue(),
      1 /* expected_count */);

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::SubframeDownload::kEntryName);
  EXPECT_EQ(1u, entries.size());

  switch (origin) {
    case Origin::kAnchorAttribute: {
      const ukm::mojom::UkmEntry* dc_entry =
          ukm_recorder.GetDocumentCreatedEntryForSourceId(
              entries.back()->source_id);
      const ukm::UkmSource* navigation_source =
          ukm_recorder.GetSourceForSourceId(*ukm_recorder.GetEntryMetric(
              dc_entry,
              ukm::builders::DocumentCreated::kNavigationSourceIdName));
      EXPECT_EQ(main_url, navigation_source->url());
    } break;
    case Origin::kNavigation: {
      ukm_recorder.ExpectEntrySourceHasUrl(entries.back(), main_url);
    } break;
  }

  ukm_recorder.ExpectEntryMetric(
      entries.back(), ukm::builders::SubframeDownload::kHasSandboxName,
      expected_flags.has_sandbox);
  ukm_recorder.ExpectEntryMetric(
      entries.back(), ukm::builders::SubframeDownload::kIsCrossOriginName,
      is_cross_origin);
  ukm_recorder.ExpectEntryMetric(
      entries.back(), ukm::builders::SubframeDownload::kIsAdFrameName,
      is_ad_frame);
  ukm_recorder.ExpectEntryMetric(
      entries.back(), ukm::builders::SubframeDownload::kHasGestureName,
      has_gesture);
}

INSTANTIATE_TEST_CASE_P(
    /* no prefix */,
    SubframeDownloadFlagsBrowserTest,
    ::testing::Combine(
        ::testing::Values(Origin::kNavigation, Origin::kAnchorAttribute),
        ::testing::Bool(),
        ::testing::Values(
            SandboxOption::kNoSandbox,
            SandboxOption::kDisallowDownloadsWithoutUserActivation,
            SandboxOption::kAllowDownloadsWithoutUserActivation),
        ::testing::Bool(),
        ::testing::Bool(),
        ::testing::Bool()));
