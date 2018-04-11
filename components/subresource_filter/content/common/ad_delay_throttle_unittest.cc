// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/common/ad_delay_throttle.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/test/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/subresource_filter/core/common/common_features.h"
#include "content/public/common/weak_wrapper_shared_url_loader_factory.h"
#include "content/public/test/throttling_url_loader_test_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/test/test_url_loader_client.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace subresource_filter {

// Use a TestURLLoaderFactory with a real ThrottlingURLLoader + the delay
// throttle.
class AdDelayThrottleTest : public testing::Test {
 public:
  AdDelayThrottleTest()
      : scoped_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME),
        client_(std::make_unique<network::TestURLLoaderClient>()),
        shared_factory_(
            base::MakeRefCounted<content::WeakWrapperSharedURLLoaderFactory>(
                &loader_factory_)) {
    scoped_ad_tagging_.InitAndEnableFeature(kAdTagging);
  }
  ~AdDelayThrottleTest() override {}

  void SetUp() override {
    scoped_features_.InitAndEnableFeature(kDelayUnsafeAds);
  }

 protected:
  std::unique_ptr<network::mojom::URLLoaderClient> CreateLoaderAndStart(
      const GURL& url,
      std::unique_ptr<AdDelayThrottle> throttle) {
    network::ResourceRequest request;
    request.url = url;
    std::vector<std::unique_ptr<content::URLLoaderThrottle>> throttles;
    throttles.push_back(std::move(throttle));
    auto loader = content::CreateThrottlingLoaderAndStart(
        shared_factory_, std::move(throttles), 0 /* routing_id */,
        0 /* request_id */, 0 /* options */, &request, client_.get(),
        TRAFFIC_ANNOTATION_FOR_TESTS, base::ThreadTaskRunnerHandle::Get());
    return loader;
  }

  base::TimeDelta GetExpectedDelay() const {
    return base::TimeDelta::FromMilliseconds(
        base::GetFieldTrialParamByFeatureAsInt(
            kDelayUnsafeAds, kInsecureDelayParam,
            AdDelayThrottle::kDefaultDelay.InMilliseconds()));
  }

  base::test::ScopedTaskEnvironment scoped_environment_;
  std::unique_ptr<network::TestURLLoaderClient> client_;
  network::TestURLLoaderFactory loader_factory_;

  base::test::ScopedFeatureList scoped_features_;

 private:
  // Use a separate ScopedFeatureList to enable ad tagging, as
  // InitAndEnableFeaturesWithParameters does not support enabling multiple
  // features.
  base::test::ScopedFeatureList scoped_ad_tagging_;
  scoped_refptr<content::WeakWrapperSharedURLLoaderFactory> shared_factory_;

  DISALLOW_COPY_AND_ASSIGN(AdDelayThrottleTest);
};

// Metadata provider that by default, provides metadata indicating that the
// request is an ad.
class MockMetadataProvider : public AdDelayThrottle::MetadataProvider {
 public:
  MockMetadataProvider() {}

  void set_is_ad_request(bool is_ad_request) { is_ad_request_ = is_ad_request; }

  // AdDelayThrottle::MetadataProvider:
  bool IsAdRequest() override { return is_ad_request_; }

 private:
  bool is_ad_request_ = true;

  DISALLOW_COPY_AND_ASSIGN(MockMetadataProvider);
};

//  This test harness is identical to the AdDelayThrottleTest but is
//  parameterized by a bool that enables or disables the feature wholesale.
class AdDelayThrottleEnabledParamTest
    : public AdDelayThrottleTest,
      public ::testing::WithParamInterface<bool> {
  void SetUp() override {
    if (GetParam()) {
      scoped_features_.InitAndEnableFeature(kDelayUnsafeAds);
    } else {
      scoped_features_.InitAndDisableFeature(kDelayUnsafeAds);
    }
  }
};

TEST_F(AdDelayThrottleTest, NoFeature_NoDelay) {
  base::test::ScopedFeatureList scoped_disable;
  scoped_disable.InitAndDisableFeature(kDelayUnsafeAds);

  AdDelayThrottle::Factory factory;
  auto throttle = factory.MaybeCreate(std::make_unique<MockMetadataProvider>());
  EXPECT_NE(nullptr, throttle);
  std::string url = "http://example.test/ad.js";
  loader_factory_.AddResponse(url, "var ads = 1;");
  std::unique_ptr<network::mojom::URLLoaderClient> loader_client =
      CreateLoaderAndStart(GURL(url), std::move(throttle));
  scoped_environment_.RunUntilIdle();

  EXPECT_TRUE(client_->has_received_completion());
}

TEST_F(AdDelayThrottleTest, NoAdTagging_NoDelay) {
  base::test::ScopedFeatureList scoped_disable;
  scoped_disable.InitAndDisableFeature(kAdTagging);

  AdDelayThrottle::Factory factory;
  auto throttle = factory.MaybeCreate(std::make_unique<MockMetadataProvider>());
  EXPECT_NE(nullptr, throttle);
  std::string url = "http://example.test/ad.js";
  loader_factory_.AddResponse(url, "var ads = 1;");
  std::unique_ptr<network::mojom::URLLoaderClient> loader_client =
      CreateLoaderAndStart(GURL(url), std::move(throttle));
  scoped_environment_.RunUntilIdle();

  EXPECT_TRUE(client_->has_received_completion());
}

TEST_F(AdDelayThrottleTest, InsecureNonAd_NoDelay) {
  AdDelayThrottle::Factory factory;
  auto metadata = std::make_unique<MockMetadataProvider>();
  metadata->set_is_ad_request(false);
  auto throttle = factory.MaybeCreate(std::move(metadata));
  EXPECT_NE(nullptr, throttle);
  std::string url = "http://example.test/ad.js";
  loader_factory_.AddResponse(url, "var ads = 1;");
  std::unique_ptr<network::mojom::URLLoaderClient> loader_client =
      CreateLoaderAndStart(GURL(url), std::move(throttle));
  scoped_environment_.RunUntilIdle();

  EXPECT_TRUE(client_->has_received_completion());
}

TEST_F(AdDelayThrottleTest, SecureAdRequest_NoDelay) {
  AdDelayThrottle::Factory factory;
  auto throttle = factory.MaybeCreate(std::make_unique<MockMetadataProvider>());
  EXPECT_NE(nullptr, throttle);
  std::string url = "https://example.test/ad.js";
  loader_factory_.AddResponse(url, "var ads = 1;");
  std::unique_ptr<network::mojom::URLLoaderClient> loader_client =
      CreateLoaderAndStart(GURL(url), std::move(throttle));
  scoped_environment_.RunUntilIdle();

  EXPECT_TRUE(client_->has_received_completion());
}

TEST_F(AdDelayThrottleTest, InsecureAdRequest_Delay) {
  AdDelayThrottle::Factory factory;
  auto throttle = factory.MaybeCreate(std::make_unique<MockMetadataProvider>());
  EXPECT_NE(nullptr, throttle);
  std::string url = "http://example.test/ad.js";
  loader_factory_.AddResponse(url, "var ads = 1;");
  std::unique_ptr<network::mojom::URLLoaderClient> loader_client =
      CreateLoaderAndStart(GURL(url), std::move(throttle));
  scoped_environment_.RunUntilIdle();

  EXPECT_FALSE(client_->has_received_completion());
  scoped_environment_.FastForwardBy(GetExpectedDelay());
  EXPECT_TRUE(client_->has_received_completion());
}

TEST_F(AdDelayThrottleTest, DelayFromFieldTrialParam) {
  base::test::ScopedFeatureList scoped_params;
  scoped_params.InitAndEnableFeatureWithParameters(
      kDelayUnsafeAds, {{kInsecureDelayParam, "100"}});
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(100), GetExpectedDelay());

  AdDelayThrottle::Factory factory;
  auto throttle = factory.MaybeCreate(std::make_unique<MockMetadataProvider>());
  std::string url = "http://example.test/ad.js";
  loader_factory_.AddResponse(url, "var ads = 1;");
  std::unique_ptr<network::mojom::URLLoaderClient> loader_client =
      CreateLoaderAndStart(GURL(url), std::move(throttle));
  scoped_environment_.RunUntilIdle();

  EXPECT_FALSE(client_->has_received_completion());

  // Ensure that we are using the greater delay here by first fast forwarding
  // just before the expected delay.
  base::TimeDelta non_triggering_delay = base::TimeDelta::FromMilliseconds(99);
  scoped_environment_.FastForwardBy(non_triggering_delay);
  EXPECT_FALSE(client_->has_received_completion());
  scoped_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(1));
  EXPECT_TRUE(client_->has_received_completion());
}

TEST_F(AdDelayThrottleTest, InsecureAdRequestRedirect_Delay) {
  AdDelayThrottle::Factory factory;
  auto throttle = factory.MaybeCreate(std::make_unique<MockMetadataProvider>());
  EXPECT_NE(nullptr, throttle);
  const GURL url("https://example.test/ad.js");

  net::RedirectInfo redirect_info;
  redirect_info.status_code = 301;
  redirect_info.new_url = GURL("http://example.test/ad.js");
  network::TestURLLoaderFactory::Redirects redirects{
      {redirect_info, network::ResourceResponseHead()}};
  loader_factory_.AddResponse(url, network::ResourceResponseHead(),
                              "var ads = 1;",
                              network::URLLoaderCompletionStatus(), redirects);
  std::unique_ptr<network::mojom::URLLoaderClient> loader_client =
      CreateLoaderAndStart(GURL(url), std::move(throttle));
  scoped_environment_.RunUntilIdle();

  EXPECT_FALSE(client_->has_received_completion());
  scoped_environment_.FastForwardBy(GetExpectedDelay());
  EXPECT_TRUE(client_->has_received_completion());
}

TEST_F(AdDelayThrottleTest, InsecureRedirectChain_DelaysOnce) {
  AdDelayThrottle::Factory factory;
  auto throttle = factory.MaybeCreate(std::make_unique<MockMetadataProvider>());
  EXPECT_NE(nullptr, throttle);
  const GURL url("http://example.test/ad.js");

  net::RedirectInfo redirect_info;
  redirect_info.status_code = 301;
  redirect_info.new_url = GURL("http://example2.test/ad.js");
  network::TestURLLoaderFactory::Redirects redirects{
      {redirect_info, network::ResourceResponseHead()}};
  loader_factory_.AddResponse(url, network::ResourceResponseHead(),
                              "var ads = 1;",
                              network::URLLoaderCompletionStatus(), redirects);
  std::unique_ptr<network::mojom::URLLoaderClient> loader_client =
      CreateLoaderAndStart(GURL(url), std::move(throttle));
  scoped_environment_.RunUntilIdle();

  EXPECT_FALSE(client_->has_received_completion());
  scoped_environment_.FastForwardBy(GetExpectedDelay());
  EXPECT_TRUE(client_->has_received_completion());
}

TEST_F(AdDelayThrottleTest, DestroyBeforeDelay) {
  AdDelayThrottle::Factory factory;
  auto throttle = factory.MaybeCreate(std::make_unique<MockMetadataProvider>());
  EXPECT_NE(nullptr, throttle);
  std::string url = "http://example.test/ad.js";
  loader_factory_.AddResponse(url, "var ads = 1;");
  std::unique_ptr<network::mojom::URLLoaderClient> loader_client =
      CreateLoaderAndStart(GURL(url), std::move(throttle));

  scoped_environment_.RunUntilIdle();
  EXPECT_FALSE(client_->has_received_completion());

  loader_client.reset();
  scoped_environment_.FastForwardBy(GetExpectedDelay());
}

TEST_F(AdDelayThrottleTest, AdDiscoveredAfterRedirect) {
  AdDelayThrottle::Factory factory;
  auto metadata = std::make_unique<MockMetadataProvider>();
  MockMetadataProvider* raw_metadata = metadata.get();
  metadata->set_is_ad_request(false);
  auto throttle = factory.MaybeCreate(std::move(metadata));
  const GURL url("http://example.test/ad.js");

  net::RedirectInfo redirect_info;
  redirect_info.status_code = 301;
  redirect_info.new_url = GURL("http://example2.test/ad.js");
  network::TestURLLoaderFactory::Redirects redirects{
      {redirect_info, network::ResourceResponseHead()}};
  loader_factory_.AddResponse(url, network::ResourceResponseHead(),
                              "var ads = 1;",
                              network::URLLoaderCompletionStatus(), redirects);

  std::unique_ptr<network::mojom::URLLoaderClient> loader_client =
      CreateLoaderAndStart(GURL(url), std::move(throttle));

  raw_metadata->set_is_ad_request(true);
  scoped_environment_.RunUntilIdle();
  EXPECT_FALSE(client_->has_received_completion());
  scoped_environment_.FastForwardBy(GetExpectedDelay());
  EXPECT_TRUE(client_->has_received_completion());
}

// Note: the AdDelayThrottle supports MetadataProviders that update IsAdRequest
// bit after redirects, but not all MetadataProviders necessarily support that.
TEST_F(AdDelayThrottleTest, AdDiscoveredAfterSecureRedirect) {
  AdDelayThrottle::Factory factory;
  auto metadata = std::make_unique<MockMetadataProvider>();
  MockMetadataProvider* raw_metadata = metadata.get();
  metadata->set_is_ad_request(false);
  auto throttle = factory.MaybeCreate(std::move(metadata));

  // Request an insecure non-ad request to a secure ad request. Since part of
  // the journey was over an insecure channel, the request should be delayed.
  const GURL url("http://example.test/ad.js");
  net::RedirectInfo redirect_info;
  redirect_info.status_code = 301;
  redirect_info.new_url = GURL("https://example.test/ad.js");
  network::TestURLLoaderFactory::Redirects redirects{
      {redirect_info, network::ResourceResponseHead()}};
  loader_factory_.AddResponse(url, network::ResourceResponseHead(),
                              "var ads = 1;",
                              network::URLLoaderCompletionStatus(), redirects);

  std::unique_ptr<network::mojom::URLLoaderClient> loader_client =
      CreateLoaderAndStart(GURL(url), std::move(throttle));

  raw_metadata->set_is_ad_request(true);
  scoped_environment_.RunUntilIdle();
  EXPECT_FALSE(client_->has_received_completion());
  scoped_environment_.FastForwardBy(GetExpectedDelay());
  EXPECT_TRUE(client_->has_received_completion());
}

// Make sure metrics are logged when the feature is enabled and disabled.
TEST_P(AdDelayThrottleEnabledParamTest, SecureMetrics) {
  AdDelayThrottle::Factory factory;
  const GURL insecure_url("http://example.test/ad.js");
  const GURL secure_url("https://example.test/ad.js");
  loader_factory_.AddResponse(insecure_url.spec(), "foo");
  loader_factory_.AddResponse(secure_url.spec(), "foo");

  const char kSecureHistogram[] = "SubresourceFilter.AdDelay.SecureInfo";
  {
    base::HistogramTester histograms;
    {
      auto throttle =
          factory.MaybeCreate(std::make_unique<MockMetadataProvider>());
      client_ = std::make_unique<network::TestURLLoaderClient>();
      auto loader = CreateLoaderAndStart(insecure_url, std::move(throttle));
      scoped_environment_.FastForwardUntilNoTasksRemain();
    }
    histograms.ExpectUniqueSample(
        kSecureHistogram,
        static_cast<int>(AdDelayThrottle::SecureInfo::kInsecureAd), 1);
  }
  {
    base::HistogramTester histograms;
    {
      auto throttle =
          factory.MaybeCreate(std::make_unique<MockMetadataProvider>());
      client_ = std::make_unique<network::TestURLLoaderClient>();
      auto loader = CreateLoaderAndStart(secure_url, std::move(throttle));
      scoped_environment_.FastForwardUntilNoTasksRemain();
    }
    histograms.ExpectUniqueSample(
        kSecureHistogram,
        static_cast<int>(AdDelayThrottle::SecureInfo::kSecureAd), 1);
  }
  {
    base::HistogramTester histograms;
    {
      auto non_ad_metadata = std::make_unique<MockMetadataProvider>();
      non_ad_metadata->set_is_ad_request(false);
      client_ = std::make_unique<network::TestURLLoaderClient>();
      auto throttle = factory.MaybeCreate(std::move(non_ad_metadata));
      auto loader = CreateLoaderAndStart(insecure_url, std::move(throttle));
      scoped_environment_.FastForwardUntilNoTasksRemain();
    }
    histograms.ExpectUniqueSample(
        kSecureHistogram,
        static_cast<int>(AdDelayThrottle::SecureInfo::kInsecureNonAd), 1);
  }
  {
    base::HistogramTester histograms;
    {
      auto non_ad_metadata = std::make_unique<MockMetadataProvider>();
      non_ad_metadata->set_is_ad_request(false);
      auto throttle = factory.MaybeCreate(std::move(non_ad_metadata));
      client_ = std::make_unique<network::TestURLLoaderClient>();
      auto loader = CreateLoaderAndStart(secure_url, std::move(throttle));
      scoped_environment_.FastForwardUntilNoTasksRemain();
    }
    histograms.ExpectUniqueSample(
        kSecureHistogram,
        static_cast<int>(AdDelayThrottle::SecureInfo::kSecureNonAd), 1);
  }
}

INSTANTIATE_TEST_CASE_P(,
                        AdDelayThrottleEnabledParamTest,
                        ::testing::Values(true, false));

}  // namespace subresource_filter
