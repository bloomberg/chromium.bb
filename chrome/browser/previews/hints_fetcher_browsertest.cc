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
#include "base/task/thread_pool/thread_pool.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/engagement/site_engagement_service.h"
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
    base::ThreadPool::GetInstance()->FlushForTesting();
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

    https_url_ = https_server_->GetURL("/hint_setup.html");
    ASSERT_TRUE(https_url_.SchemeIs(url::kHttpsScheme));

    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    cmd->AppendSwitch("enable-spdy-proxy-auth");

    // Due to race conditions, it's possible that blacklist data is not loaded
    // at the time of first navigation. That may prevent Preview from
    // triggering, and causing the test to flake.
    cmd->AppendSwitch(previews::switches::kIgnorePreviewsBlacklist);
    cmd->AppendSwitch("purge_hint_cache_store");

    // Set up OptimizationGuideServiceURL, this does not enable HintsFetching,
    // only provides the URL.
    cmd->AppendSwitchASCII(previews::switches::kOptimizationGuideServiceURL,
                           https_server_->base_url().spec());
    cmd->AppendSwitchASCII(previews::switches::kFetchHintsOverride,
                           "example1.com, example2.com");
  }

  // Creates hint data for the |hint_setup_url|'s so that OnHintsUpdated in
  // Previews Optimization Guide is called and HintsFetch can be tested.
  void SetUpComponentUpdateHints(const GURL& hint_setup_url) {
    const optimization_guide::HintsComponentInfo& component_info =
        test_hints_component_creator_.CreateHintsComponentInfoWithPageHints(
            optimization_guide::proto::NOSCRIPT, {hint_setup_url.host()}, {});

    // Register a QuitClosure for when the next hint update is started below.
    base::RunLoop run_loop;
    PreviewsServiceFactory::GetForProfile(
        Profile::FromBrowserContext(browser()
                                        ->tab_strip_model()
                                        ->GetActiveWebContents()
                                        ->GetBrowserContext()))
        ->previews_ui_service()
        ->previews_decider_impl()
        ->previews_opt_guide()
        ->ListenForNextUpdateForTesting(run_loop.QuitClosure());

    g_browser_process->optimization_guide_service()->MaybeUpdateHintsComponent(
        component_info);

    run_loop.Run();
  }

  // Seeds the Site Engagement Service with two HTTP and two HTTPS sites for the
  // current profile.
  void SeedSiteEngagementService() {
    SiteEngagementService* service = SiteEngagementService::Get(
        Profile::FromBrowserContext(browser()
                                        ->tab_strip_model()
                                        ->GetActiveWebContents()
                                        ->GetBrowserContext()));
    GURL https_url1("https://images.google.com/");
    service->AddPointsForTesting(https_url1, 15);

    GURL https_url2("https://news.google.com/");
    service->AddPointsForTesting(https_url2, 3);

    GURL http_url1("http://photos.google.com/");
    service->AddPointsForTesting(http_url1, 21);

    GURL http_url2("http://maps.google.com/");
    service->AddPointsForTesting(http_url2, 2);
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

  optimization_guide::testing::TestHintsComponentCreator
      test_hints_component_creator_;

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

// Issues with multiple profiles likely cause the site enagement service-based
// tests to flake.
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)
#define DISABLE_ON_WIN_MAC_CHROMESOS(x) DISABLED_##x
#else
#define DISABLE_ON_WIN_MAC_CHROMESOS(x) x
#endif

// This test creates new browser with no profile and loads a random page with
// the feature flags enables the PreviewsOnePlatformHints. We confirm that the
// top_host_provider_impl executes and does not crash by checking UMA
// histograms for the total number of TopEngagementSites and
// the total number of sites returned controlled by the experiments flag
// |max_oneplatform_update_hosts|.
IN_PROC_BROWSER_TEST_F(OptimizationGuideServiceHintsFetcherBrowserTest,
                       OptimizationGuideServiceHintsFetcher) {
  const base::HistogramTester* histogram_tester = GetHistogramTester();

  // Whitelist NoScript for https_url()'s' host.
  SetUpComponentUpdateHints(https_url());

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

// This test creates a new browser and seeds the Site Engagement Service with
// both HTTP and HTTPS sites. The test confirms that PreviewsTopHostProviderImpl
// used by PreviewsOptimizationGuide to provide a list of hosts to HintsFetcher
// only returns HTTPS-schemed hosts. We verify this with the UMA histogram
// logged when the GetHintsRequest is made to the remote Optimization Guide
// Service.
IN_PROC_BROWSER_TEST_F(
    OptimizationGuideServiceHintsFetcherBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMESOS(PreviewsTopHostProviderHTTPSOnly)) {
  const base::HistogramTester* histogram_tester = GetHistogramTester();

  // Adds two HTTP and two HTTPS sites into the Site Engagement Service.
  SeedSiteEngagementService();

  // This forces the hint cache to be initialized and hints to be fetched.
  // Whitelist NoScript for https_url()'s' host.
  SetUpComponentUpdateHints(https_url());

  // Expect that the browser initialization will record at least one sample as
  // Hints Fetching is enabled. This also ensures that the histograms have been
  // updated to verify the correct number of hosts that hints will be requested
  // for.
  EXPECT_GE(RetryForHistogramUntilCountReached(
                histogram_tester,
                "Previews.HintsFetcher.GetHintsRequest.HostCount", 1),
            1);

  // Only the 2 HTTPS hosts should be requested hints for.
  histogram_tester->ExpectBucketCount(
      "Previews.HintsFetcher.GetHintsRequest.HostCount", 2, 1);
}
