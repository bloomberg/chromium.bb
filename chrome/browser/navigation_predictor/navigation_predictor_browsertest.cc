// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

class NavigationPredictorBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  NavigationPredictorBrowserTest() {
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
    InProcessBrowserTest::SetUp();
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
        "AnchorElementMetrics.Visible.NumberOfAnchorElements", 2, 1);
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
  } else {
    histogram_tester.ExpectTotalCount(
        "AnchorElementMetrics.Clicked.HrefEngagementScore2", 0);
  }
}

// Test that MergeMetricsSameTargetUrl merges anchor elements having the same
// href. The html file contains two anchor elements having the same href.
IN_PROC_BROWSER_TEST_P(NavigationPredictorBrowserTest,
                       MergeMetricsSameTargetUrl) {
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
}
