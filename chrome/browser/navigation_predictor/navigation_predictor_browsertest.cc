// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/metrics/subprocess_metrics_provider.h"
#include "chrome/browser/navigation_predictor/navigation_predictor.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace {

// Retries fetching |histogram_name| until it contains at least |count| samples.
void RetryForHistogramUntilCountReached(base::HistogramTester* histogram_tester,
                                        const std::string& histogram_name,
                                        size_t count) {
  base::RunLoop().RunUntilIdle();
  for (size_t attempt = 0; attempt < 3; ++attempt) {
    const std::vector<base::Bucket> buckets =
        histogram_tester->GetAllSamples(histogram_name);
    size_t total_count = 0;
    for (const auto& bucket : buckets)
      total_count += bucket.count;
    if (total_count >= count)
      return;
    content::FetchHistogramsFromChildProcesses();
    SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
    base::RunLoop().RunUntilIdle();
  }
}

}  // namespace

class NavigationPredictorBrowserTest
    : public subresource_filter::SubresourceFilterBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  NavigationPredictorBrowserTest()
      : subresource_filter::SubresourceFilterBrowserTest() {
    const std::vector<base::Feature> features = {
        blink::features::kRecordAnchorMetricsVisible,
        blink::features::kRecordAnchorMetricsClicked};
    if (GetParam()) {
      feature_list_.InitWithFeatures(features, {});
    } else {
      feature_list_.InitWithFeatures({}, features);
    }
  }

  void SetUp() override {
    https_server_.reset(
        new net::EmbeddedTestServer(net::EmbeddedTestServer::TYPE_HTTPS));
    https_server_->ServeFilesFromSourceDirectory(
        "chrome/test/data/navigation_predictor");
    ASSERT_TRUE(https_server_->Start());

    http_server_.reset(
        new net::EmbeddedTestServer(net::EmbeddedTestServer::TYPE_HTTP));
    http_server_->ServeFilesFromSourceDirectory(
        "chrome/test/data/navigation_predictor");
    ASSERT_TRUE(http_server_->Start());

    subresource_filter::SubresourceFilterBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    subresource_filter::SubresourceFilterBrowserTest::SetUpOnMainThread();
    host_resolver()->ClearRules();
  }

  const GURL GetTestURL(const char* file) const {
    return https_server_->GetURL(file);
  }

  const GURL GetHttpTestURL(const char* file) const {
    return http_server_->GetURL(file);
  }

 private:
  std::unique_ptr<net::EmbeddedTestServer> http_server_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(NavigationPredictorBrowserTest);
};

// Enable and disable feature blink::features::kRecordAnchorMetricsVisible.
INSTANTIATE_TEST_CASE_P(, NavigationPredictorBrowserTest, testing::Bool());

// Test that with feature blink::features::kRecordAnchorMetricsVisible enabled,
// the browser process can receive anchor element metrics on page load. And when
// the feature is disabled, there is no metric received.
IN_PROC_BROWSER_TEST_P(NavigationPredictorBrowserTest, Pipeline) {
  base::HistogramTester histogram_tester;

  const GURL& url = GetTestURL("/simple_page_with_anchors.html");
  ui_test_utils::NavigateToURL(browser(), url);
  base::RunLoop().RunUntilIdle();

  if (base::FeatureList::IsEnabled(
          blink::features::kRecordAnchorMetricsVisible)) {
    histogram_tester.ExpectUniqueSample(
        "AnchorElementMetrics.Visible.NumberOfAnchorElements", 3, 1);
    // Same document anchor element should be removed after merge.
    histogram_tester.ExpectUniqueSample(
        "AnchorElementMetrics.Visible.NumberOfAnchorElementsAfterMerge", 2, 1);
  } else {
    histogram_tester.ExpectTotalCount(
        "AnchorElementMetrics.Visible.NumberOfAnchorElements", 0);
    histogram_tester.ExpectTotalCount(
        "AnchorElementMetrics.Visible.NumberOfAnchorElementsAfterMerge", 0);
  }
}

// Test that no metrics are recorded in off-the-record profiles.
IN_PROC_BROWSER_TEST_P(NavigationPredictorBrowserTest, PipelineOffTheRecord) {
  base::HistogramTester histogram_tester;

  const GURL& url = GetTestURL("/simple_page_with_anchors.html");
  Browser* incognito = CreateIncognitoBrowser();
  ui_test_utils::NavigateToURL(incognito, url);
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      "AnchorElementMetrics.Visible.NumberOfAnchorElements", 0);
  histogram_tester.ExpectTotalCount(
      "AnchorElementMetrics.Visible.NumberOfAnchorElementsAfterMerge", 0);
}

// Test that the browser does not process anchor element metrics from an http
// web page on page load.
IN_PROC_BROWSER_TEST_P(NavigationPredictorBrowserTest, PipelineHttp) {
  base::HistogramTester histogram_tester;

  const GURL& url = GetHttpTestURL("/simple_page_with_anchors.html");
  ui_test_utils::NavigateToURL(browser(), url);
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      "AnchorElementMetrics.Visible.NumberOfAnchorElements", 0);
  histogram_tester.ExpectTotalCount(
      "AnchorElementMetrics.Visible.NumberOfAnchorElementsAfterMerge", 0);
}

// Test that anchor elements within an iframe tagged as an ad are discarded when
// predicting next navigation.
IN_PROC_BROWSER_TEST_P(NavigationPredictorBrowserTest, PipelineAdsFrameTagged) {
  // iframe_ads_simple_page_with_anchors.html is an iframe referenced by
  // page_with_ads_iframe.html.
  ASSERT_NO_FATAL_FAILURE(SetRulesetToDisallowURLsWithPathSuffix(
      "iframe_ads_simple_page_with_anchors.html"));

  base::HistogramTester histogram_tester;

  GURL url = GetTestURL("/page_with_ads_iframe.html");
  ui_test_utils::NavigateToURL(browser(), url);
  base::RunLoop().RunUntilIdle();

  if (base::FeatureList::IsEnabled(
          blink::features::kRecordAnchorMetricsVisible)) {
    histogram_tester.ExpectUniqueSample(
        "AnchorElementMetrics.Visible.NumberOfAnchorElements", 5, 1);
    histogram_tester.ExpectUniqueSample(
        "AnchorElementMetrics.Visible.NumberOfAnchorElementsAfterMerge", 2, 1);

    RetryForHistogramUntilCountReached(
        &histogram_tester, "AnchorElementMetrics.IsAdFrameElement", 4);

    histogram_tester.ExpectTotalCount("AnchorElementMetrics.IsAdFrameElement",
                                      7);
    histogram_tester.ExpectBucketCount("AnchorElementMetrics.IsAdFrameElement",
                                       0 /* false */, 5);
    histogram_tester.ExpectBucketCount("AnchorElementMetrics.IsAdFrameElement",
                                       1 /* true */, 2);

  } else {
    histogram_tester.ExpectTotalCount(
        "AnchorElementMetrics.Visible.NumberOfAnchorElements", 0);
  }
}

// Test that anchor elements within an iframe not tagged as ad are not discarded
// when predicting next navigation.
IN_PROC_BROWSER_TEST_P(NavigationPredictorBrowserTest,
                       PipelineAdsFrameNotTagged) {
  base::HistogramTester histogram_tester;

  GURL url = GetTestURL("/page_with_ads_iframe.html");
  ui_test_utils::NavigateToURL(browser(), url);
  base::RunLoop().RunUntilIdle();

  if (base::FeatureList::IsEnabled(
          blink::features::kRecordAnchorMetricsVisible)) {
    histogram_tester.ExpectUniqueSample(
        "AnchorElementMetrics.Visible.NumberOfAnchorElements", 7, 1);
    histogram_tester.ExpectUniqueSample(
        "AnchorElementMetrics.Visible.NumberOfAnchorElementsAfterMerge", 2, 1);

    RetryForHistogramUntilCountReached(
        &histogram_tester, "AnchorElementMetrics.IsAdFrameElement", 7);

    histogram_tester.ExpectUniqueSample("AnchorElementMetrics.IsAdFrameElement",
                                        0 /* false */, 7);

  } else {
    histogram_tester.ExpectTotalCount(
        "AnchorElementMetrics.Visible.NumberOfAnchorElements", 0);
  }
}

// Test that navigation score of anchor elements can be calculated on page load.
IN_PROC_BROWSER_TEST_P(NavigationPredictorBrowserTest, NavigationScore) {
  base::HistogramTester histogram_tester;

  const GURL& url = GetTestURL("/simple_page_with_anchors.html");
  ui_test_utils::NavigateToURL(browser(), url);
  base::RunLoop().RunUntilIdle();

  if (base::FeatureList::IsEnabled(
          blink::features::kRecordAnchorMetricsVisible)) {
    histogram_tester.ExpectTotalCount(
        "AnchorElementMetrics.Visible.HighestNavigationScore", 1);
  } else {
    histogram_tester.ExpectTotalCount(
        "AnchorElementMetrics.Visible.HighestNavigationScore", 0);
  }
}

// Simulate a click at the anchor element.
// Test that timing info (DurationLoadToFirstClick) can be recorded.
// And that the navigation score can be looked up.
IN_PROC_BROWSER_TEST_P(NavigationPredictorBrowserTest, ClickAnchorElement) {
  base::HistogramTester histogram_tester;

  const GURL& url = GetTestURL("/simple_page_with_anchors.html");
  ui_test_utils::NavigateToURL(browser(), url);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "document.getElementById('google').click();"));
  base::RunLoop().RunUntilIdle();

  if (base::FeatureList::IsEnabled(
          blink::features::kRecordAnchorMetricsClicked)) {
    histogram_tester.ExpectTotalCount(
        "AnchorElementMetrics.Clicked.HrefEngagementScore2", 1);
    histogram_tester.ExpectTotalCount(
        "AnchorElementMetrics.Clicked.DurationLoadToFirstClick", 1);
    histogram_tester.ExpectTotalCount(
        "AnchorElementMetrics.Clicked.NavigationScore", 1);

    histogram_tester.ExpectUniqueSample(
        "NavigationPredictor.OnNonDSE.ActionTaken",
        NavigationPredictor::Action::kNone, 1);

  } else {
    histogram_tester.ExpectTotalCount(
        "AnchorElementMetrics.Clicked.HrefEngagementScore2", 0);
  }
}

// Simulate a click at the anchor element.
// Test that the action accuracy is properly recorded.
// User clicks on an anchor element that points to a origin different than the
// origin of the URL prefetched.
IN_PROC_BROWSER_TEST_P(NavigationPredictorBrowserTest,
                       ActionAccuracy_DifferentOrigin) {
  base::HistogramTester histogram_tester;

  const GURL& url = GetTestURL("/page_with_same_host_anchor_element.html");
  ui_test_utils::NavigateToURL(browser(), url);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "document.getElementById('google').click();"));
  base::RunLoop().RunUntilIdle();

  if (base::FeatureList::IsEnabled(
          blink::features::kRecordAnchorMetricsClicked)) {
    histogram_tester.ExpectUniqueSample(
        "AnchorElementMetrics.Visible.NumberOfAnchorElements", 2, 1);
    // Same document anchor element should be removed after merge.
    histogram_tester.ExpectUniqueSample(
        "AnchorElementMetrics.Visible.NumberOfAnchorElementsAfterMerge", 2, 1);
    histogram_tester.ExpectUniqueSample(
        "NavigationPredictor.OnNonDSE.ActionTaken",
        NavigationPredictor::Action::kPrefetch, 1);

    histogram_tester.ExpectTotalCount(
        "AnchorElementMetrics.Clicked.HrefEngagementScore2", 1);

    histogram_tester.ExpectUniqueSample(
        "NavigationPredictor.OnNonDSE.AccuracyActionTaken",
        NavigationPredictor::ActionAccuracy::
            kPrefetchActionClickToDifferentOrigin,
        1);

  } else {
    histogram_tester.ExpectTotalCount(
        "AnchorElementMetrics.Visible.NumberOfAnchorElements", 0);
  }
}

// Simulate a click at the anchor element.
// Test that the action accuracy is properly recorded.
// User clicks on an anchor element that points to same URL as the URL
// prefetched.
IN_PROC_BROWSER_TEST_P(NavigationPredictorBrowserTest,
                       ActionAccuracy_SameOrigin) {
  base::HistogramTester histogram_tester;

  const GURL& url = GetTestURL("/page_with_same_host_anchor_element.html");
  ui_test_utils::NavigateToURL(browser(), url);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "document.getElementById('example').click();"));
  base::RunLoop().RunUntilIdle();

  if (base::FeatureList::IsEnabled(
          blink::features::kRecordAnchorMetricsClicked)) {
    histogram_tester.ExpectUniqueSample(
        "AnchorElementMetrics.Visible.NumberOfAnchorElements", 2, 1);
    // Same document anchor element should be removed after merge.
    histogram_tester.ExpectUniqueSample(
        "AnchorElementMetrics.Visible.NumberOfAnchorElementsAfterMerge", 2, 1);
    histogram_tester.ExpectUniqueSample(
        "NavigationPredictor.OnNonDSE.ActionTaken",
        NavigationPredictor::Action::kPrefetch, 1);

    histogram_tester.ExpectTotalCount(
        "AnchorElementMetrics.Clicked.HrefEngagementScore2", 1);

    histogram_tester.ExpectUniqueSample(
        "NavigationPredictor.OnNonDSE.AccuracyActionTaken",
        NavigationPredictor::ActionAccuracy::kPrefetchActionClickToSameURL, 1);

  } else {
    histogram_tester.ExpectTotalCount(
        "AnchorElementMetrics.Visible.NumberOfAnchorElements", 0);
  }
}

// Simulate a click at the anchor element in off-the-record profile. Metrics
// should not be recorded.
IN_PROC_BROWSER_TEST_P(NavigationPredictorBrowserTest,
                       ClickAnchorElementOffTheRecord) {
  base::HistogramTester histogram_tester;

  const GURL& url = GetTestURL("/simple_page_with_anchors.html");

  Browser* incognito = CreateIncognitoBrowser();
  ui_test_utils::NavigateToURL(incognito, url);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(content::ExecuteScript(
      incognito->tab_strip_model()->GetActiveWebContents(),
      "document.getElementById('google').click();"));
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      "AnchorElementMetrics.Clicked.HrefEngagementScore2", 0);
}

// Simulate click at the anchor element.
// Test that correct area ranks are recorded.
IN_PROC_BROWSER_TEST_P(NavigationPredictorBrowserTest, AreaRank) {
  base::HistogramTester histogram_tester;

  // This test file contains 5 anchors with different size.
  const GURL& url = GetTestURL("/anchors_different_area.html");
  ui_test_utils::NavigateToURL(browser(), url);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "document.getElementById('medium').click();"));
  base::RunLoop().RunUntilIdle();

  if (base::FeatureList::IsEnabled(
          blink::features::kRecordAnchorMetricsClicked)) {
    histogram_tester.ExpectUniqueSample("AnchorElementMetrics.Clicked.AreaRank",
                                        2, 1);
    histogram_tester.ExpectTotalCount("AnchorElementMetrics.Visible.RatioArea",
                                      5);
  }
}

// Test that MergeMetricsSameTargetUrl merges anchor elements having the same
// href. The html file contains two anchor elements having the same href.
IN_PROC_BROWSER_TEST_P(NavigationPredictorBrowserTest,
                       MergeMetricsSameTargetUrl_ClickHrefWithNoMergedImage) {
  base::HistogramTester histogram_tester;

  const GURL& url = GetTestURL("/anchors_same_href.html");
  ui_test_utils::NavigateToURL(browser(), url);
  base::RunLoop().RunUntilIdle();

  if (base::FeatureList::IsEnabled(
          blink::features::kRecordAnchorMetricsVisible)) {
    histogram_tester.ExpectTotalCount("AnchorElementMetrics.Visible.RatioArea",
                                      1);
  } else {
    histogram_tester.ExpectTotalCount("AnchorElementMetrics.Visible.RatioArea",
                                      0);
  }

  EXPECT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "document.getElementById('diffHref').click();"));
  base::RunLoop().RunUntilIdle();

  // Anchor element with id 'diffHref' points to an href. No image in the
  // webpage also points to an image. So, clicking on this non-image anchor
  // element, should not be recorded as "ContainsImage".
  if (base::FeatureList::IsEnabled(
          blink::features::kRecordAnchorMetricsVisible)) {
    histogram_tester.ExpectTotalCount(
        "AnchorElementMetrics.Clicked.RatioContainsImage_ContainsImage", 0);
  }
}

// Test that MergeMetricsSameTargetUrl merges anchor elements having the same
// href. The html file contains two anchor elements having the same href.
IN_PROC_BROWSER_TEST_P(NavigationPredictorBrowserTest,
                       MergeMetricsSameTargetUrl_ClickHrefWithMergedImage) {
  base::HistogramTester histogram_tester;

  const GURL& url = GetTestURL("/anchors_same_href.html");
  ui_test_utils::NavigateToURL(browser(), url);
  base::RunLoop().RunUntilIdle();

  if (base::FeatureList::IsEnabled(
          blink::features::kRecordAnchorMetricsVisible)) {
    histogram_tester.ExpectTotalCount("AnchorElementMetrics.Visible.RatioArea",
                                      1);
  } else {
    histogram_tester.ExpectTotalCount("AnchorElementMetrics.Visible.RatioArea",
                                      0);
  }

  EXPECT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "document.getElementById('google').click();"));
  base::RunLoop().RunUntilIdle();

  // Anchor element with id 'google' points to an href. Another image in the
  // webpage also points to an image. So, even though we clicked on a non-image
  // anchor element, it should be recorded as "ContainsImage".
  if (base::FeatureList::IsEnabled(
          blink::features::kRecordAnchorMetricsVisible)) {
    histogram_tester.ExpectTotalCount(
        "AnchorElementMetrics.Clicked.RatioContainsImage_ContainsImage", 1);
  }
}

IN_PROC_BROWSER_TEST_P(NavigationPredictorBrowserTest,
                       AnchorElementClickedOnSearchEnginePage) {
  static const char kShortName[] = "test";
  static const char kSearchURL[] = "/anchors_same_href.html?q={searchTerms}";

  TemplateURLService* model =
      TemplateURLServiceFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(model);
  search_test_utils::WaitForTemplateURLServiceToLoad(model);
  ASSERT_TRUE(model->loaded());

  TemplateURLData data;
  data.SetShortName(base::ASCIIToUTF16(kShortName));
  data.SetKeyword(data.short_name());
  data.SetURL(GetTestURL(kSearchURL).spec());

  TemplateURL* template_url = model->Add(std::make_unique<TemplateURL>(data));
  ASSERT_TRUE(template_url);
  model->SetUserSelectedDefaultSearchProvider(template_url);

  base::HistogramTester histogram_tester;

  const GURL& url = GetTestURL("/anchors_same_href.html?q=cats");
  ui_test_utils::NavigateToURL(browser(), url);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "document.getElementById('google').click();"));
  base::RunLoop().RunUntilIdle();

  // Anchor element with id 'google' points to an href that's on a different
  // host.
  if (base::FeatureList::IsEnabled(
          blink::features::kRecordAnchorMetricsVisible)) {
    histogram_tester.ExpectTotalCount(
        "AnchorElementMetrics.Clicked.RatioContainsImage_ContainsImage", 1);
    histogram_tester.ExpectUniqueSample(
        "AnchorElementMetrics.Clicked.OnDSE.SameHost", 0, 1);
  }
}

IN_PROC_BROWSER_TEST_P(NavigationPredictorBrowserTest,
                       AnchorElementClickedOnNonSearchEnginePage) {
  static const char kShortName[] = "test";
  static const char kSearchURL[] = "/somne_other_url.html?q={searchTerms}";

  TemplateURLService* model =
      TemplateURLServiceFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(model);
  search_test_utils::WaitForTemplateURLServiceToLoad(model);
  ASSERT_TRUE(model->loaded());

  TemplateURLData data;
  data.SetShortName(base::ASCIIToUTF16(kShortName));
  data.SetKeyword(data.short_name());
  data.SetURL(GetTestURL(kSearchURL).spec());

  TemplateURL* template_url = model->Add(std::make_unique<TemplateURL>(data));
  ASSERT_TRUE(template_url);
  model->SetUserSelectedDefaultSearchProvider(template_url);

  base::HistogramTester histogram_tester;

  const GURL& url = GetTestURL("/anchors_same_href.html?q=cats");
  ui_test_utils::NavigateToURL(browser(), url);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "document.getElementById('google').click();"));
  base::RunLoop().RunUntilIdle();

  // Anchor element with id 'google' points to an href that's on a different
  // host.
  if (base::FeatureList::IsEnabled(
          blink::features::kRecordAnchorMetricsVisible)) {
    histogram_tester.ExpectTotalCount(
        "AnchorElementMetrics.Clicked.RatioContainsImage_ContainsImage", 1);
    histogram_tester.ExpectUniqueSample(
        "AnchorElementMetrics.Clicked.OnNonDSE.SameHost", 0, 1);
  }
}

IN_PROC_BROWSER_TEST_P(NavigationPredictorBrowserTest,
                       ActionPrefetch_NoSameHostAnchorElement) {
  base::HistogramTester histogram_tester;

  const GURL& url = GetTestURL("/simple_page_with_anchors.html");
  ui_test_utils::NavigateToURL(browser(), url);
  base::RunLoop().RunUntilIdle();

  if (base::FeatureList::IsEnabled(
          blink::features::kRecordAnchorMetricsVisible)) {
    histogram_tester.ExpectUniqueSample(
        "AnchorElementMetrics.Visible.NumberOfAnchorElements", 3, 1);
    // Same document anchor element should be removed after merge.
    histogram_tester.ExpectUniqueSample(
        "AnchorElementMetrics.Visible.NumberOfAnchorElementsAfterMerge", 2, 1);
    histogram_tester.ExpectUniqueSample(
        "NavigationPredictor.OnNonDSE.ActionTaken",
        NavigationPredictor::Action::kNone, 1);
  } else {
    histogram_tester.ExpectTotalCount(
        "AnchorElementMetrics.Visible.NumberOfAnchorElements", 0);
  }
}

IN_PROC_BROWSER_TEST_P(NavigationPredictorBrowserTest,
                       ActionPrefetch_SameHostAnchorElement) {
  base::HistogramTester histogram_tester;

  const GURL& url = GetTestURL("/page_with_same_host_anchor_element.html");
  ui_test_utils::NavigateToURL(browser(), url);
  base::RunLoop().RunUntilIdle();

  if (base::FeatureList::IsEnabled(
          blink::features::kRecordAnchorMetricsVisible)) {
    histogram_tester.ExpectUniqueSample(
        "AnchorElementMetrics.Visible.NumberOfAnchorElements", 2, 1);
    // Same document anchor element should be removed after merge.
    histogram_tester.ExpectUniqueSample(
        "AnchorElementMetrics.Visible.NumberOfAnchorElementsAfterMerge", 2, 1);
    histogram_tester.ExpectUniqueSample(
        "NavigationPredictor.OnNonDSE.ActionTaken",
        NavigationPredictor::Action::kPrefetch, 1);
  } else {
    histogram_tester.ExpectTotalCount(
        "AnchorElementMetrics.Visible.NumberOfAnchorElements", 0);
  }
}
