// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/navigation_predictor/navigation_predictor.h"

#include <map>

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/loader/navigation_predictor.mojom.h"
#include "url/gurl.h"

namespace {

class TestNavigationPredictor : public NavigationPredictor {
 public:
  explicit TestNavigationPredictor(
      mojo::InterfaceRequest<AnchorElementMetricsHost> request,
      content::RenderFrameHost* render_frame_host)
      : NavigationPredictor(render_frame_host), binding_(this) {
    binding_.Bind(std::move(request));
  }

  ~TestNavigationPredictor() override {}

  const std::map<GURL, int>& GetAreaRankMap() const { return area_rank_map_; }

 private:
  double CalculateAnchorNavigationScore(
      const blink::mojom::AnchorElementMetrics& metrics,
      double document_engagement_score,
      double target_engagement_score,
      int area_rank) const override {
    area_rank_map_.emplace(std::make_pair(metrics.target_url, area_rank));
    return 100 * metrics.ratio_area;
  }

  // Maps from target URL to area rank of the anchor element.
  mutable std::map<GURL, int> area_rank_map_;

  // Used to bind Mojo interface
  mojo::Binding<AnchorElementMetricsHost> binding_;
};

class NavigationPredictorTest : public ChromeRenderViewHostTestHarness {
 public:
  NavigationPredictorTest() = default;
  ~NavigationPredictorTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    predictor_service_helper_ = std::make_unique<TestNavigationPredictor>(
        mojo::MakeRequest(&predictor_service_), main_rfh());
  }

  void TearDown() override { ChromeRenderViewHostTestHarness::TearDown(); }

  // Helper function to generate mojom metrics.
  blink::mojom::AnchorElementMetricsPtr CreateMetricsPtr(
      const std::string& source_url,
      const std::string& target_url,
      float ratio_area) const {
    auto metrics = blink::mojom::AnchorElementMetrics::New();
    metrics->source_url = GURL(source_url);
    metrics->target_url = GURL(target_url);
    metrics->ratio_area = ratio_area;
    return metrics;
  }

  blink::mojom::AnchorElementMetricsHost* predictor_service() const {
    return predictor_service_.get();
  }

  TestNavigationPredictor* predictor_service_helper() const {
    return predictor_service_helper_.get();
  }

 private:
  blink::mojom::AnchorElementMetricsHostPtr predictor_service_;
  std::unique_ptr<TestNavigationPredictor> predictor_service_helper_;
};

}  // namespace

// Basic test to check the ReportAnchorElementMetricsOnClick method can be
// called.
TEST_F(NavigationPredictorTest, ReportAnchorElementMetricsOnClick) {
  base::HistogramTester histogram_tester;

  auto metrics =
      CreateMetricsPtr("http://example.com", "https://google.com", 0.1);
  predictor_service()->ReportAnchorElementMetricsOnClick(std::move(metrics));
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      "AnchorElementMetrics.Clicked.HrefEngagementScore2", 1);
}

// Test that ReportAnchorElementMetricsOnLoad method can be called.
TEST_F(NavigationPredictorTest, ReportAnchorElementMetricsOnLoad) {
  base::HistogramTester histogram_tester;

  auto metrics =
      CreateMetricsPtr("https://example.com", "http://google.com", 0.1);
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

  auto metrics =
      CreateMetricsPtr("ftp://example.com", "https://google.com", 0.1);
  predictor_service()->ReportAnchorElementMetricsOnClick(std::move(metrics));
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      "AnchorElementMetrics.Clicked.HrefEngagementScore2", 0);
}

// Test that if source/target url is not http or https, no navigation score will
// be calculated.
TEST_F(NavigationPredictorTest, BadUrlReportAnchorElementMetricsOnLoad) {
  base::HistogramTester histogram_tester;

  auto metrics =
      CreateMetricsPtr("https://example.com", "ftp://google.com", 0.1);
  std::vector<blink::mojom::AnchorElementMetricsPtr> metrics_vector;
  metrics_vector.push_back(std::move(metrics));
  predictor_service()->ReportAnchorElementMetricsOnLoad(
      std::move(metrics_vector));
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      "AnchorElementMetrics.Visible.HighestNavigationScore", 0);
}

// In this test, multiple anchor element metrics are sent to
// ReportAnchorElementMetricsOnLoad. Test that CalculateAnchorNavigationScore
// works, and that highest navigation score can be recorded correctly.
TEST_F(NavigationPredictorTest, MultipleAnchorElementMetricsOnLoad) {
  base::HistogramTester histogram_tester;

  const std::string source = "http://example.com";
  const std::string href_xlarge = "http://example.com/xlarge";
  const std::string href_large = "http://google.com/large";
  const std::string href_medium = "http://google.com/medium";
  const std::string href_small = "http://google.com/small";
  const std::string href_xsmall = "http://google.com/xsmall";

  std::vector<blink::mojom::AnchorElementMetricsPtr> metrics;
  metrics.push_back(CreateMetricsPtr(source, href_xsmall, 0.01));
  metrics.push_back(CreateMetricsPtr(source, href_large, 0.08));
  metrics.push_back(CreateMetricsPtr(source, href_xlarge, 0.1));
  metrics.push_back(CreateMetricsPtr(source, href_small, 0.02));
  metrics.push_back(CreateMetricsPtr(source, href_medium, 0.05));

  int number_of_mertics_sent = metrics.size();
  predictor_service()->ReportAnchorElementMetricsOnLoad(std::move(metrics));
  base::RunLoop().RunUntilIdle();

  const std::map<GURL, int>& area_rank_map =
      predictor_service_helper()->GetAreaRankMap();
  EXPECT_EQ(number_of_mertics_sent, static_cast<int>(area_rank_map.size()));
  EXPECT_EQ(0, area_rank_map.find(GURL(href_xlarge))->second);
  EXPECT_EQ(1, area_rank_map.find(GURL(href_large))->second);
  EXPECT_EQ(2, area_rank_map.find(GURL(href_medium))->second);
  EXPECT_EQ(3, area_rank_map.find(GURL(href_small))->second);
  EXPECT_EQ(4, area_rank_map.find(GURL(href_xsmall))->second);

  // The highest score is 100 (scale factor) * 0.1 (largest area) = 10.
  histogram_tester.ExpectUniqueSample(
      "AnchorElementMetrics.Visible.HighestNavigationScore", 10, 1);
  histogram_tester.ExpectTotalCount("AnchorElementMetrics.Visible.RatioArea",
                                    5);
}
