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

  blink::mojom::AnchorElementMetricsHost* predictor_service() const {
    return predictor_service_.get();
  }

 private:
  blink::mojom::AnchorElementMetricsHostPtr predictor_service_;
};

}  // namespace

// Basic test to check the UpdateAnchorElementMetrics method can be called.
TEST_F(NavigationPredictorTest, UpdateAnchorElementMetrics) {
  base::HistogramTester histogram_tester;

  auto metrics = blink::mojom::AnchorElementMetrics::New();
  metrics->target_url = GURL("https://example.com");
  predictor_service()->UpdateAnchorElementMetrics(std::move(metrics));
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      "AnchorElementMetrics.Clicked.HrefEngagementScore2", 1);
}

// Test that if source url is not http or https, no score will be calculated.
TEST_F(NavigationPredictorTest, BadUrlUpdateAnchorElementMetrics) {
  base::HistogramTester histogram_tester;

  auto metrics = blink::mojom::AnchorElementMetrics::New();
  metrics->target_url = GURL("ftp://example.com");
  predictor_service()->UpdateAnchorElementMetrics(std::move(metrics));
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      "AnchorElementMetrics.Clicked.HrefEngagementScore2", 0);
}
