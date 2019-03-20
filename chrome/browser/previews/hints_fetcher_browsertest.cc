// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/task/task_scheduler/task_scheduler.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/subprocess_metrics_provider.h"
#include "chrome/browser/previews/previews_service.h"
#include "chrome/browser/previews/previews_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/optimization_guide/hints_component_info.h"
#include "components/optimization_guide/optimization_guide_service.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/optimization_guide/test_hints_component_creator.h"
#include "components/previews/content/previews_decider_impl.h"
#include "components/previews/content/previews_optimization_guide.h"
#include "components/previews/content/previews_ui_service.h"
#include "components/previews/core/previews_black_list.h"
#include "components/previews/core/previews_constants.h"
#include "components/previews/core/previews_features.h"
#include "components/previews/core/previews_switches.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/cpp/network_quality_tracker.h"

namespace {

// Fetch and calculate the total number of samples from all the bins for
// |histogram_name|. Note: from some browertests run (such as chromeos) there
// might be two profiles created, and this will return the total sample count
// across profiles.
int GetTotalHistogramSamples(const base::HistogramTester* histogram_tester,
                             const std::string& histogram_name) {
  std::vector<base::Bucket> buckets =
      histogram_tester->GetAllSamples(histogram_name);
  int total = 0;
  for (const auto& bucket : buckets)
    total += bucket.count;

  return total;
}

// Retries fetching |histogram_name| until it contains at least |count| samples.
int RetryForHistogramUntilCountReached(
    const base::HistogramTester* histogram_tester,
    const std::string& histogram_name,
    int count) {
  int total = 0;
  while (true) {
    base::TaskScheduler::GetInstance()->FlushForTesting();
    base::RunLoop().RunUntilIdle();

    total = GetTotalHistogramSamples(histogram_tester, histogram_name);
    if (total >= count)
      return total;
  }
}

}  // namespace

// This test class sets up everything but does not enable any features.
class OptimizationGuideServiceNoHintsFetcherBrowserTest
    : public InProcessBrowserTest {
 public:
  OptimizationGuideServiceNoHintsFetcherBrowserTest() = default;
  ~OptimizationGuideServiceNoHintsFetcherBrowserTest() override = default;

  void SetUp() override {
    https_server_.reset(
        new net::EmbeddedTestServer(net::EmbeddedTestServer::TYPE_HTTPS));
    https_server_->ServeFilesFromSourceDirectory("chrome/test/data/previews");
    https_server_->RegisterRequestMonitor(
        base::BindRepeating(&OptimizationGuideServiceNoHintsFetcherBrowserTest::
                                MonitorResourceRequest,
                            base::Unretained(this)));
    ASSERT_TRUE(https_server_->Start());

    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    cmd->AppendSwitch("enable-spdy-proxy-auth");

    // Due to race conditions, it's possible that blacklist data is not loaded
    // at the time of first navigation. That may prevent Preview from
    // triggering, and causing the test to flake.
    cmd->AppendSwitch(previews::switches::kIgnorePreviewsBlacklist);

    // Set up OptimizationGuideServiceURL, this does not enable HintsFetching,
    // only provides the URL.
    cmd->AppendSwitchASCII(previews::switches::kOptimizationGuideServiceURL,
                           https_server_->base_url().spec());
  }

  const GURL& https_url() const { return https_url_; }
  const base::HistogramTester* GetHistogramTester() {
    return &histogram_tester_;
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;

 private:
  // Called by |https_server_|.
  void MonitorResourceRequest(const net::test_server::HttpRequest& request) {
    // The request by HintsFetcher request should happen and be a POST request.
    EXPECT_EQ(request.method, net::test_server::METHOD_POST);
    // TODO(mcrouse): Test server returns 404 for now as HintsFetcher does not
    // currently handle responses.
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(https_server_->ShutdownAndWaitUntilComplete());

    InProcessBrowserTest::TearDownOnMainThread();
  }

  GURL https_url_;

  base::HistogramTester histogram_tester_;

  DISALLOW_COPY_AND_ASSIGN(OptimizationGuideServiceNoHintsFetcherBrowserTest);
};

// This test class enables OnePlatform Hints.
class OptimizationGuideServiceHintsFetcherBrowserTest
    : public OptimizationGuideServiceNoHintsFetcherBrowserTest {
 public:
  OptimizationGuideServiceHintsFetcherBrowserTest() = default;

  ~OptimizationGuideServiceHintsFetcherBrowserTest() override = default;

  void SetUp() override {
    // Enable OptimizationHintsFetching with |kOptimizationHintsFetching|.
    scoped_feature_list_.InitWithFeatures(
        {previews::features::kPreviews, previews::features::kOptimizationHints,
         previews::features::kOptimizationHintsFetching},
        {});

    // Call to inherited class to match same set up with feature flags added.
    OptimizationGuideServiceNoHintsFetcherBrowserTest::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(OptimizationGuideServiceHintsFetcherBrowserTest);
};
// This test creates new browser with no profile and loads a random page with
// the feature flags enables the PreviewsOnePlatformHints. We confirm that the
// top_host_provider_impl executes and does not crash by checking UMA
// histograms for the total number of TopEngagementSites and
// the total number of sites returned controlled by the experiments flag
// |max_oneplatform_update_hosts|.
IN_PROC_BROWSER_TEST_F(OptimizationGuideServiceHintsFetcherBrowserTest,
                       OptimizationGuideServiceHintsFetcher) {
  const base::HistogramTester* histogram_tester = GetHistogramTester();

  // Expect that the browser initialization will record at least one sample
  // in each of the follow histograms as One Platform Hints are enabled.
  EXPECT_GE(RetryForHistogramUntilCountReached(
                histogram_tester,
                "Previews.HintsFetcher.GetHintsRequest.HostCount", 1),
            1);
}

IN_PROC_BROWSER_TEST_F(OptimizationGuideServiceNoHintsFetcherBrowserTest,
                       OptimizationGuideServiceNoHintsFetcher) {
  const base::HistogramTester* histogram_tester = GetHistogramTester();

  // Expect that the histogram for HintsFetcher to be 0 because the OnePlatform
  // is not enabled.
  histogram_tester->ExpectTotalCount(
      "Previews.HintsFetcher.GetHintsRequest.HostCount", 0);
}
