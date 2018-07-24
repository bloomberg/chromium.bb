// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/navigation_predictor/navigation_predictor.h"

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

class NavigationPredictorTest : public ChromeRenderViewHostTestHarness {
 public:
  NavigationPredictorTest() = default;
  ~NavigationPredictorTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    NavigationPredictor::Create(mojo::MakeRequest(&predictor_service_),
                                main_rfh());
  }

  void TearDown() override { ChromeRenderViewHostTestHarness::TearDown(); }

  // Helper function to generate mojom metrics.
  blink::mojom::AnchorElementMetricsPtr CreateMetricsPtr(
      const std::string& source_url,
      const std::string& target_url) const {
    auto metrics = blink::mojom::AnchorElementMetrics::New();
    metrics->source_url = GURL(source_url);
    metrics->target_url = GURL(target_url);
    return metrics;
  }

  blink::mojom::AnchorElementMetricsHost* predictor_service() const {
    return predictor_service_.get();
  }

 private:
  blink::mojom::AnchorElementMetricsHostPtr predictor_service_;
};

}  // namespace

// Basic test to check the ReportAnchorElementMetricsOnClick method can be
// called.
TEST_F(NavigationPredictorTest, ReportAnchorElementMetricsOnClick) {
  base::HistogramTester histogram_tester;

  auto metrics = CreateMetricsPtr("http://example.com", "https://google.com");
  predictor_service()->ReportAnchorElementMetricsOnClick(std::move(metrics));
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      "AnchorElementMetrics.Clicked.HrefEngagementScore2", 1);
}

// Test that ReportAnchorElementMetricsOnLoad method can be called.
TEST_F(NavigationPredictorTest, ReportAnchorElementMetricsOnLoad) {
  base::HistogramTester histogram_tester;

  auto metrics = CreateMetricsPtr("https://example.com", "http://google.com");
  std::vector<blink::mojom::AnchorElementMetricsPtr> metrics_vector;
  metrics_vector.push_back(std::move(metrics));
  predictor_service()->ReportAnchorElementMetricsOnLoad(
      std::move(metrics_vector));
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      "AnchorElementMetrics.Visible.HighestNavigationScore", 1);
}

// Test that if source/target url is not http or https, no score will be
// calculated.
TEST_F(NavigationPredictorTest, BadUrlReportAnchorElementMetricsOnClick) {
  base::HistogramTester histogram_tester;

  auto metrics = CreateMetricsPtr("ftp://example.com", "https://google.com");
  predictor_service()->ReportAnchorElementMetricsOnClick(std::move(metrics));
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      "AnchorElementMetrics.Clicked.HrefEngagementScore2", 0);
}

// Test that if source/target url is not http or https, no navigation score will
// be calculated.
TEST_F(NavigationPredictorTest, BadUrlReportAnchorElementMetricsOnLoad) {
  base::HistogramTester histogram_tester;

  auto metrics = CreateMetricsPtr("https://example.com", "ftp://google.com");
  std::vector<blink::mojom::AnchorElementMetricsPtr> metrics_vector;
  metrics_vector.push_back(std::move(metrics));
  predictor_service()->ReportAnchorElementMetricsOnLoad(
      std::move(metrics_vector));
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      "AnchorElementMetrics.Visible.HighestNavigationScore", 0);
}
