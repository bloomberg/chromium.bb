// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/memory/ptr_util.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/browser/net/predictor.h"
#include "chrome/browser/predictors/loading_predictor_config.h"
#include "chrome/browser/predictors/resource_prefetch_common.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/variations/variations_params_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/network_change_notifier.h"
#include "testing/gtest/include/gtest/gtest.h"

using chrome_browser_net::NetworkPredictionOptions;
using NetPredictor = chrome_browser_net::Predictor;
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

class LoadingPredictorConfigTest : public testing::Test {
 public:
  LoadingPredictorConfigTest();

  void SetPreference(NetworkPredictionOptions value) {
    profile_->GetPrefs()->SetInteger(prefs::kNetworkPredictionOptions, value);
  }

  bool IsNetPredictorEnabled() {
    std::unique_ptr<NetPredictor> predictor = base::WrapUnique(
        NetPredictor::CreatePredictor(true /* simple_shutdown */));
    bool is_enabled = predictor->PredictorEnabled();
    predictor->Shutdown();
    return is_enabled;
  }

  void TestIsPrefetchEnabledForOrigin(const LoadingPredictorConfig& config,
                                      HintOrigin origin) {
    EXPECT_TRUE(config.IsLearningEnabled());
    EXPECT_TRUE(config.IsPrefetchingEnabledForOrigin(profile_.get(), origin));
    EXPECT_FALSE(config.IsPreconnectEnabledForSomeOrigin(profile_.get()));
  }

  void TestIsPrefetchLearning(const LoadingPredictorConfig& config) {
    EXPECT_TRUE(config.IsLearningEnabled());
    EXPECT_TRUE(config.is_host_learning_enabled);
    EXPECT_FALSE(config.is_origin_learning_enabled);
    EXPECT_FALSE(config.IsPrefetchingEnabledForSomeOrigin(profile_.get()));
    EXPECT_FALSE(config.IsPreconnectEnabledForSomeOrigin(profile_.get()));
  }

  void TestIsDefaultExtraConfig(const LoadingPredictorConfig& config) {
    EXPECT_FALSE(config.IsLowConfidenceForTest());
    EXPECT_FALSE(config.IsHighConfidenceForTest());
    EXPECT_FALSE(config.IsMoreResourcesEnabledForTest());
    EXPECT_FALSE(config.IsSmallDBEnabledForTest());
    EXPECT_FALSE(config.is_url_learning_enabled);
    EXPECT_GT(config.min_resource_hits_to_trigger_prefetch, 1U);
  }

 protected:
  content::TestBrowserThreadBundle test_browser_thread_bundle_;
  std::unique_ptr<TestingProfile> profile_;
};

LoadingPredictorConfigTest::LoadingPredictorConfigTest()
    : profile_(new TestingProfile()) {}

TEST_F(LoadingPredictorConfigTest, IsDisabledByDefault) {
  LoadingPredictorConfig config;
  EXPECT_FALSE(MaybeEnableResourcePrefetching(&config));
  EXPECT_FALSE(MaybeEnableSpeculativePreconnect(&config));

  EXPECT_FALSE(config.IsLearningEnabled());
  EXPECT_FALSE(config.IsPrefetchingEnabledForSomeOrigin(profile_.get()));
  EXPECT_FALSE(config.IsPreconnectEnabledForSomeOrigin(profile_.get()));

  TestIsDefaultExtraConfig(config);
}

TEST_F(LoadingPredictorConfigTest, EnablePrefetchingLearning) {
  variations::testing::VariationParamsManager params_manager(
      "dummy-trial", {{kModeParamName, kLearningMode}},
      {kSpeculativeResourcePrefetchingFeatureName});

  LoadingPredictorConfig config;
  EXPECT_TRUE(MaybeEnableResourcePrefetching(&config));
  TestIsPrefetchLearning(config);
  TestIsDefaultExtraConfig(config);
}

TEST_F(LoadingPredictorConfigTest, EnablePreconnectLearning) {
  variations::testing::VariationParamsManager params_manager(
      "dummy-trial", {{kModeParamName, kLearningMode}},
      {kSpeculativePreconnectFeatureName});

  LoadingPredictorConfig config;
  EXPECT_TRUE(MaybeEnableSpeculativePreconnect(&config));

  EXPECT_TRUE(config.IsLearningEnabled());
  EXPECT_TRUE(config.is_origin_learning_enabled);
  EXPECT_FALSE(config.is_host_learning_enabled);
  EXPECT_FALSE(config.should_disable_other_preconnects);
  EXPECT_TRUE(IsNetPredictorEnabled());
  EXPECT_FALSE(config.IsPrefetchingEnabledForSomeOrigin(profile_.get()));
  EXPECT_FALSE(config.IsPreconnectEnabledForSomeOrigin(profile_.get()));
  TestIsDefaultExtraConfig(config);
}

TEST_F(LoadingPredictorConfigTest, EnablePrefetch) {
  variations::testing::VariationParamsManager params_manager(
      "dummy-trial", {{kModeParamName, kPrefetchingMode}},
      {kSpeculativeResourcePrefetchingFeatureName});

  LoadingPredictorConfig config;
  EXPECT_TRUE(MaybeEnableResourcePrefetching(&config));
  TestIsPrefetchEnabledForOrigin(config, HintOrigin::EXTERNAL);
  TestIsPrefetchEnabledForOrigin(config, HintOrigin::NAVIGATION);
  TestIsDefaultExtraConfig(config);
}

TEST_F(LoadingPredictorConfigTest, EnablePrefetchExternalOnly) {
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

TEST_F(LoadingPredictorConfigTest, EnablePreconnect) {
  variations::testing::VariationParamsManager params_manager(
      "dummy-trial", {{kModeParamName, kPreconnectMode}},
      {kSpeculativePreconnectFeatureName});

  LoadingPredictorConfig config;
  EXPECT_TRUE(MaybeEnableSpeculativePreconnect(&config));

  EXPECT_TRUE(config.IsLearningEnabled());
  EXPECT_TRUE(config.should_disable_other_preconnects);
  EXPECT_FALSE(IsNetPredictorEnabled());
  EXPECT_FALSE(config.IsPrefetchingEnabledForSomeOrigin(profile_.get()));
  EXPECT_TRUE(config.IsPreconnectEnabledForSomeOrigin(profile_.get()));
  TestIsDefaultExtraConfig(config);
}

TEST_F(LoadingPredictorConfigTest, EnableNoPreconnect) {
  variations::testing::VariationParamsManager params_manager(
      "dummy-trial", {{kModeParamName, kNoPreconnectMode}},
      {kSpeculativePreconnectFeatureName});

  LoadingPredictorConfig config;
  EXPECT_FALSE(MaybeEnableSpeculativePreconnect(&config));

  EXPECT_FALSE(config.IsLearningEnabled());
  EXPECT_TRUE(config.should_disable_other_preconnects);
  EXPECT_FALSE(IsNetPredictorEnabled());
  EXPECT_FALSE(config.IsPrefetchingEnabledForSomeOrigin(profile_.get()));
  EXPECT_FALSE(config.IsPreconnectEnabledForSomeOrigin(profile_.get()));
  TestIsDefaultExtraConfig(config);
}

TEST_F(LoadingPredictorConfigTest, EnableUrlLearning) {
  variations::testing::VariationParamsManager params_manager(
      "dummy-trial",
      {{kModeParamName, kLearningMode}, {kEnableUrlLearningParamName, "true"}},
      {kSpeculativeResourcePrefetchingFeatureName});

  LoadingPredictorConfig config;
  EXPECT_TRUE(MaybeEnableResourcePrefetching(&config));
  TestIsPrefetchLearning(config);
  EXPECT_TRUE(config.is_url_learning_enabled);
}

// Verifies whether prefetching is disabled according to the network type. But
// learning should not be disabled by network.
TEST_F(LoadingPredictorConfigTest, RespectsNetworkSettings) {
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
