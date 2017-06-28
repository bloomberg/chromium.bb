// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/memory/ptr_util.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/browser/predictors/resource_prefetch_common.h"
#include "chrome/browser/predictors/resource_prefetch_predictor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/variations/variations_params_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/network_change_notifier.h"
#include "testing/gtest/include/gtest/gtest.h"

using chrome_browser_net::NetworkPredictionOptions;
using net::NetworkChangeNotifier;

namespace {

class MockNetworkChangeNotifierWIFI : public NetworkChangeNotifier {
 public:
  ConnectionType GetCurrentConnectionType() const override {
    return NetworkChangeNotifier::CONNECTION_WIFI;
  }
};

class MockNetworkChangeNotifier4G : public NetworkChangeNotifier {
 public:
  ConnectionType GetCurrentConnectionType() const override {
    return NetworkChangeNotifier::CONNECTION_4G;
  }
};

}  // namespace

namespace predictors {

class ResourcePrefetchCommonTest : public testing::Test {
 public:
  ResourcePrefetchCommonTest();

  void SetPreference(NetworkPredictionOptions value) {
    profile_->GetPrefs()->SetInteger(prefs::kNetworkPredictionOptions, value);
  }

  void TestIsPrefetchEnabledForOrigin(const LoadingPredictorConfig& config,
                                      HintOrigin origin) {
    EXPECT_TRUE(config.IsLearningEnabled());
    EXPECT_TRUE(config.IsPrefetchingEnabledForOrigin(profile_.get(), origin));
  }

  void TestIsPrefetchLearning(const LoadingPredictorConfig& config) {
    EXPECT_TRUE(config.IsLearningEnabled());
    EXPECT_FALSE(config.IsPrefetchingEnabledForOrigin(profile_.get(),
                                                      HintOrigin::EXTERNAL));
    EXPECT_FALSE(config.IsPrefetchingEnabledForOrigin(profile_.get(),
                                                      HintOrigin::NAVIGATION));
    EXPECT_GT(config.min_resource_hits_to_trigger_prefetch, 1U);
  }

  void TestIsDefaultExtraConfig(const LoadingPredictorConfig& config) {
    EXPECT_FALSE(config.IsLowConfidenceForTest());
    EXPECT_FALSE(config.IsHighConfidenceForTest());
    EXPECT_FALSE(config.IsMoreResourcesEnabledForTest());
    EXPECT_FALSE(config.IsSmallDBEnabledForTest());
    EXPECT_FALSE(config.is_url_learning_enabled);
    EXPECT_FALSE(config.is_manifests_enabled);
    EXPECT_FALSE(config.is_origin_learning_enabled);
    EXPECT_GT(config.min_resource_hits_to_trigger_prefetch, 1U);
  }

 protected:
  content::TestBrowserThreadBundle test_browser_thread_bundle_;
  std::unique_ptr<TestingProfile> profile_;
};

ResourcePrefetchCommonTest::ResourcePrefetchCommonTest()
    : profile_(new TestingProfile()) {}

TEST_F(ResourcePrefetchCommonTest, IsDisabledByDefault) {
  LoadingPredictorConfig config;
  EXPECT_FALSE(MaybeEnableResourcePrefetching(&config));

  EXPECT_FALSE(config.IsLearningEnabled());
  EXPECT_FALSE(config.IsPrefetchingEnabledForOrigin(profile_.get(),
                                                    HintOrigin::EXTERNAL));
  EXPECT_FALSE(config.IsPrefetchingEnabledForOrigin(profile_.get(),
                                                    HintOrigin::NAVIGATION));

  TestIsDefaultExtraConfig(config);
}

TEST_F(ResourcePrefetchCommonTest, EnableLearning) {
  variations::testing::VariationParamsManager params_manager(
      "dummy-trial", {{kModeParamName, kLearningMode}},
      {kSpeculativeResourcePrefetchingFeatureName});

  LoadingPredictorConfig config;
  EXPECT_TRUE(MaybeEnableResourcePrefetching(&config));
  TestIsPrefetchLearning(config);
  TestIsDefaultExtraConfig(config);
}

TEST_F(ResourcePrefetchCommonTest, EnablePrefetch) {
  variations::testing::VariationParamsManager params_manager(
      "dummy-trial", {{kModeParamName, kPrefetchingMode}},
      {kSpeculativeResourcePrefetchingFeatureName});

  LoadingPredictorConfig config;
  EXPECT_TRUE(MaybeEnableResourcePrefetching(&config));
  TestIsPrefetchEnabledForOrigin(config, HintOrigin::EXTERNAL);
  TestIsPrefetchEnabledForOrigin(config, HintOrigin::NAVIGATION);
  TestIsDefaultExtraConfig(config);
}

TEST_F(ResourcePrefetchCommonTest, EnablePrefetchExternalOnly) {
  variations::testing::VariationParamsManager params_manager(
      "dummy-trial", {{kModeParamName, kExternalPrefetchingMode}},
      {kSpeculativeResourcePrefetchingFeatureName});

  LoadingPredictorConfig config;
  EXPECT_TRUE(MaybeEnableResourcePrefetching(&config));
  TestIsPrefetchEnabledForOrigin(config, HintOrigin::EXTERNAL);
  EXPECT_FALSE(config.IsPrefetchingEnabledForOrigin(profile_.get(),
                                                    HintOrigin::NAVIGATION));
  TestIsDefaultExtraConfig(config);
}

TEST_F(ResourcePrefetchCommonTest, EnableUrlLearning) {
  variations::testing::VariationParamsManager params_manager(
      "dummy-trial",
      {{kModeParamName, kLearningMode}, {kEnableUrlLearningParamName, "true"}},
      {kSpeculativeResourcePrefetchingFeatureName});

  LoadingPredictorConfig config;
  EXPECT_TRUE(MaybeEnableResourcePrefetching(&config));
  TestIsPrefetchLearning(config);
  EXPECT_TRUE(config.is_url_learning_enabled);
}

TEST_F(ResourcePrefetchCommonTest, EnableManifests) {
  variations::testing::VariationParamsManager params_manager(
      "dummy-trial",
      {{kModeParamName, kLearningMode}, {kEnableManifestsParamName, "true"}},
      {kSpeculativeResourcePrefetchingFeatureName});

  LoadingPredictorConfig config;
  EXPECT_TRUE(MaybeEnableResourcePrefetching(&config));
  TestIsPrefetchLearning(config);
  EXPECT_TRUE(config.is_manifests_enabled);
}

// Verifies whether prefetching is disabled according to the network type. But
// learning should not be disabled by network.
TEST_F(ResourcePrefetchCommonTest, RespectsNetworkSettings) {
  variations::testing::VariationParamsManager params_manager(
      "dummy-trial", {{kModeParamName, kPrefetchingMode}},
      {kSpeculativeResourcePrefetchingFeatureName});

  LoadingPredictorConfig config;
  EXPECT_TRUE(MaybeEnableResourcePrefetching(&config));
  TestIsPrefetchEnabledForOrigin(config, HintOrigin::EXTERNAL);
  TestIsDefaultExtraConfig(config);

  // Set preference to WIFI_ONLY: prefetch when not on cellular.
  SetPreference(NetworkPredictionOptions::NETWORK_PREDICTION_WIFI_ONLY);
  {
    std::unique_ptr<NetworkChangeNotifier> mock(
        new MockNetworkChangeNotifierWIFI);
    TestIsPrefetchEnabledForOrigin(config, HintOrigin::EXTERNAL);
  }
  {
    std::unique_ptr<NetworkChangeNotifier> mock(
        new MockNetworkChangeNotifier4G);
    TestIsPrefetchLearning(config);
  }

  // Set preference to NEVER: never prefetch.
  SetPreference(NetworkPredictionOptions::NETWORK_PREDICTION_NEVER);
  {
    std::unique_ptr<NetworkChangeNotifier> mock(
        new MockNetworkChangeNotifierWIFI);
    TestIsPrefetchLearning(config);
  }
  {
    std::unique_ptr<NetworkChangeNotifier> mock(
        new MockNetworkChangeNotifier4G);
    TestIsPrefetchLearning(config);
  }
}

}  // namespace predictors
