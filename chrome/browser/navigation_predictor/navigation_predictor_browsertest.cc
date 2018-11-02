// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/metrics/subprocess_metrics_provider.h"
#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
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
    test_server_.ServeFilesFromSourceDirectory(
        "chrome/test/data/navigation_predictor");
    ASSERT_TRUE(test_server_.Start());
    subresource_filter::SubresourceFilterBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    subresource_filter::SubresourceFilterBrowserTest::SetUpOnMainThread();
    host_resolver()->ClearRules();
  }

  const GURL GetTestURL(const char* file) const {
    return test_server_.GetURL(file);
  }

 private:
  net::EmbeddedTestServer test_server_;
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
        "AnchorElementMetrics.Visible.NumberOfAnchorElements", 2, 1);

    RetryForHistogramUntilCountReached(
        &histogram_tester, "AnchorElementMetrics.IsAdFrameElement", 4);

    histogram_tester.ExpectTotalCount("AnchorElementMetrics.IsAdFrameElement",
                                      4);
    histogram_tester.ExpectBucketCount("AnchorElementMetrics.IsAdFrameElement",
                                       0 /* false */, 2);
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
        "AnchorElementMetrics.Visible.NumberOfAnchorElements", 4, 1);

    RetryForHistogramUntilCountReached(
        &histogram_tester, "AnchorElementMetrics.IsAdFrameElement", 4);

    histogram_tester.ExpectUniqueSample("AnchorElementMetrics.IsAdFrameElement",
                                        0 /* false */, 4);

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

  } else {
    histogram_tester.ExpectTotalCount(
        "AnchorElementMetrics.Clicked.HrefEngagementScore2", 0);
  }
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
