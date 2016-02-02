// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/statistics_recorder.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/browser/predictors/resource_prefetch_common.h"
#include "chrome/browser/predictors/resource_prefetch_predictor.h"
#include "chrome/browser/predictors/resource_prefetch_predictor_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/variations/entropy_provider.h"
#include "content/public/test/test_browser_thread.h"
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

} // namespace

namespace predictors {

class ResourcePrefetchCommonTest : public testing::Test {
 public:
  ResourcePrefetchCommonTest();
  void SetUp() override;

  void CreateTestFieldTrial(const std::string& name,
                            const std::string& group_name) {
    base::FieldTrial* trial = base::FieldTrialList::CreateFieldTrial(
        name, group_name);
    trial->group();
  }

  void SetPreference(NetworkPredictionOptions value) {
    profile_->GetPrefs()->SetInteger(prefs::kNetworkPredictionOptions, value);
  }

  void TestIsPrefetchDisabled(ResourcePrefetchPredictorConfig& config) {
    EXPECT_FALSE(config.IsLearningEnabled());
    EXPECT_FALSE(config.IsPrefetchingEnabled(profile_.get()));
    EXPECT_FALSE(config.IsURLLearningEnabled());
    EXPECT_FALSE(config.IsHostLearningEnabled());
    EXPECT_FALSE(config.IsURLPrefetchingEnabled(profile_.get()));
    EXPECT_FALSE(config.IsHostPrefetchingEnabled(profile_.get()));
  }

  void TestIsPrefetchEnabled(ResourcePrefetchPredictorConfig& config) {
    EXPECT_TRUE(config.IsLearningEnabled());
    EXPECT_TRUE(config.IsPrefetchingEnabled(profile_.get()));
    EXPECT_TRUE(config.IsURLLearningEnabled());
    EXPECT_TRUE(config.IsHostLearningEnabled());
    EXPECT_TRUE(config.IsURLPrefetchingEnabled(profile_.get()));
    EXPECT_TRUE(config.IsHostPrefetchingEnabled(profile_.get()));
  }

  void TestIsPrefetchLearning(ResourcePrefetchPredictorConfig& config) {
    EXPECT_TRUE(config.IsLearningEnabled());
    EXPECT_FALSE(config.IsPrefetchingEnabled(profile_.get()));
    EXPECT_TRUE(config.IsURLLearningEnabled());
    EXPECT_TRUE(config.IsHostLearningEnabled());
    EXPECT_FALSE(config.IsURLPrefetchingEnabled(profile_.get()));
    EXPECT_FALSE(config.IsHostPrefetchingEnabled(profile_.get()));
  }

  void TestIsDefaultExtraConfig(ResourcePrefetchPredictorConfig& config) {
    EXPECT_FALSE(config.IsLowConfidenceForTest());
    EXPECT_FALSE(config.IsHighConfidenceForTest());
    EXPECT_FALSE(config.IsMoreResourcesEnabledForTest());
    EXPECT_FALSE(config.IsSmallDBEnabledForTest());
  }

 protected:
  base::MessageLoop loop_;
  content::TestBrowserThread ui_thread_;
  scoped_ptr<TestingProfile> profile_;

 private:
  scoped_ptr<base::FieldTrialList> field_trial_list_;
};

ResourcePrefetchCommonTest::ResourcePrefetchCommonTest()
    : loop_(base::MessageLoop::TYPE_DEFAULT),
      ui_thread_(content::BrowserThread::UI, &loop_),
      profile_(new TestingProfile()) { }

void ResourcePrefetchCommonTest::SetUp() {
  field_trial_list_.reset(new base::FieldTrialList(
      new metrics::SHA1EntropyProvider("ResourcePrefetchCommonTest")));
  base::StatisticsRecorder::Initialize();
}

TEST_F(ResourcePrefetchCommonTest, FieldTrialNotSpecified) {
  ResourcePrefetchPredictorConfig config;
  EXPECT_FALSE(
      IsSpeculativeResourcePrefetchingEnabled(profile_.get(), &config));
  TestIsPrefetchDisabled(config);
}

TEST_F(ResourcePrefetchCommonTest, FieldTrialPrefetchingDisabled) {
  CreateTestFieldTrial("SpeculativeResourcePrefetching",
                       "Prefetching=Disabled");
  ResourcePrefetchPredictorConfig config;
  EXPECT_FALSE(
      IsSpeculativeResourcePrefetchingEnabled(profile_.get(), &config));
  TestIsPrefetchDisabled(config);
}

TEST_F(ResourcePrefetchCommonTest, FieldTrialLearningHost) {
  CreateTestFieldTrial("SpeculativeResourcePrefetching",
                       "Prefetching=Learning:Predictor=Host");
  ResourcePrefetchPredictorConfig config;
  EXPECT_TRUE(IsSpeculativeResourcePrefetchingEnabled(profile_.get(), &config));
  EXPECT_TRUE(config.IsLearningEnabled());
  EXPECT_FALSE(config.IsPrefetchingEnabled(profile_.get()));
  EXPECT_FALSE(config.IsURLLearningEnabled());
  EXPECT_TRUE(config.IsHostLearningEnabled());
  EXPECT_FALSE(config.IsURLPrefetchingEnabled(profile_.get()));
  EXPECT_FALSE(config.IsHostPrefetchingEnabled(profile_.get()));
  TestIsDefaultExtraConfig(config);
}

TEST_F(ResourcePrefetchCommonTest, FieldTrialLearningURL) {
  CreateTestFieldTrial("SpeculativeResourcePrefetching",
                       "Prefetching=Learning:Predictor=Url");
  ResourcePrefetchPredictorConfig config;
  EXPECT_TRUE(IsSpeculativeResourcePrefetchingEnabled(profile_.get(), &config));
  EXPECT_TRUE(config.IsLearningEnabled());
  EXPECT_FALSE(config.IsPrefetchingEnabled(profile_.get()));
  EXPECT_TRUE(config.IsURLLearningEnabled());
  EXPECT_FALSE(config.IsHostLearningEnabled());
  EXPECT_FALSE(config.IsURLPrefetchingEnabled(profile_.get()));
  EXPECT_FALSE(config.IsHostPrefetchingEnabled(profile_.get()));
  TestIsDefaultExtraConfig(config);
}

TEST_F(ResourcePrefetchCommonTest, FieldTrialLearning) {
  CreateTestFieldTrial("SpeculativeResourcePrefetching",
                       "Prefetching=Learning");
  ResourcePrefetchPredictorConfig config;
  EXPECT_TRUE(IsSpeculativeResourcePrefetchingEnabled(profile_.get(), &config));
  TestIsPrefetchLearning(config);
  TestIsDefaultExtraConfig(config);
}

TEST_F(ResourcePrefetchCommonTest, FieldTrialPrefetchingHost) {
  CreateTestFieldTrial("SpeculativeResourcePrefetching",
                       "Prefetching=Enabled:Predictor=Host");
  ResourcePrefetchPredictorConfig config;
  EXPECT_TRUE(IsSpeculativeResourcePrefetchingEnabled(profile_.get(), &config));
  EXPECT_TRUE(config.IsLearningEnabled());
  EXPECT_TRUE(config.IsPrefetchingEnabled(profile_.get()));
  EXPECT_FALSE(config.IsURLLearningEnabled());
  EXPECT_TRUE(config.IsHostLearningEnabled());
  EXPECT_FALSE(config.IsURLPrefetchingEnabled(profile_.get()));
  EXPECT_TRUE(config.IsHostPrefetchingEnabled(profile_.get()));
  TestIsDefaultExtraConfig(config);
}

TEST_F(ResourcePrefetchCommonTest, FieldTrialPrefetchingURL) {
  CreateTestFieldTrial("SpeculativeResourcePrefetching",
                       "Prefetching=Enabled:Predictor=Url");
  ResourcePrefetchPredictorConfig config;
  EXPECT_TRUE(IsSpeculativeResourcePrefetchingEnabled(profile_.get(), &config));
  EXPECT_TRUE(config.IsLearningEnabled());
  EXPECT_TRUE(config.IsPrefetchingEnabled(profile_.get()));
  EXPECT_TRUE(config.IsURLLearningEnabled());
  EXPECT_FALSE(config.IsHostLearningEnabled());
  EXPECT_TRUE(config.IsURLPrefetchingEnabled(profile_.get()));
  EXPECT_FALSE(config.IsHostPrefetchingEnabled(profile_.get()));
  TestIsDefaultExtraConfig(config);
}

TEST_F(ResourcePrefetchCommonTest, FieldTrialPrefetching) {
  CreateTestFieldTrial("SpeculativeResourcePrefetching", "Prefetching=Enabled");
  ResourcePrefetchPredictorConfig config;
  EXPECT_TRUE(IsSpeculativeResourcePrefetchingEnabled(profile_.get(), &config));
  TestIsPrefetchEnabled(config);
  TestIsDefaultExtraConfig(config);
}

TEST_F(ResourcePrefetchCommonTest, FieldTrialPrefetchingLowConfidence) {
  CreateTestFieldTrial("SpeculativeResourcePrefetching",
                       "Prefetching=Enabled:Confidence=Low");
  ResourcePrefetchPredictorConfig config;
  EXPECT_TRUE(IsSpeculativeResourcePrefetchingEnabled(profile_.get(), &config));
  TestIsPrefetchEnabled(config);
  EXPECT_TRUE(config.IsLowConfidenceForTest());
  EXPECT_FALSE(config.IsHighConfidenceForTest());
  EXPECT_FALSE(config.IsMoreResourcesEnabledForTest());
  EXPECT_FALSE(config.IsSmallDBEnabledForTest());
}

TEST_F(ResourcePrefetchCommonTest, FieldTrialPrefetchingHighConfidence) {
  CreateTestFieldTrial("SpeculativeResourcePrefetching",
                       "Prefetching=Enabled:Confidence=High");
  ResourcePrefetchPredictorConfig config;
  EXPECT_TRUE(IsSpeculativeResourcePrefetchingEnabled(profile_.get(), &config));
  TestIsPrefetchEnabled(config);
  EXPECT_FALSE(config.IsLowConfidenceForTest());
  EXPECT_TRUE(config.IsHighConfidenceForTest());
  EXPECT_FALSE(config.IsMoreResourcesEnabledForTest());
  EXPECT_FALSE(config.IsSmallDBEnabledForTest());
}

TEST_F(ResourcePrefetchCommonTest, FieldTrialPrefetchingMoreResources) {
  CreateTestFieldTrial("SpeculativeResourcePrefetching",
                       "Prefetching=Learning:MoreResources=Enabled");
  ResourcePrefetchPredictorConfig config;
  EXPECT_TRUE(IsSpeculativeResourcePrefetchingEnabled(profile_.get(), &config));
  TestIsPrefetchLearning(config);
  EXPECT_FALSE(config.IsLowConfidenceForTest());
  EXPECT_FALSE(config.IsHighConfidenceForTest());
  EXPECT_TRUE(config.IsMoreResourcesEnabledForTest());
  EXPECT_FALSE(config.IsSmallDBEnabledForTest());
}

TEST_F(ResourcePrefetchCommonTest, FieldTrialLearningSmallDB) {
  CreateTestFieldTrial("SpeculativeResourcePrefetching",
                       "Prefetching=Learning:SmallDB=Enabled");
  ResourcePrefetchPredictorConfig config;
  EXPECT_TRUE(IsSpeculativeResourcePrefetchingEnabled(profile_.get(), &config));
  TestIsPrefetchLearning(config);
  EXPECT_FALSE(config.IsLowConfidenceForTest());
  EXPECT_FALSE(config.IsHighConfidenceForTest());
  EXPECT_FALSE(config.IsMoreResourcesEnabledForTest());
  EXPECT_TRUE(config.IsSmallDBEnabledForTest());
}

TEST_F(ResourcePrefetchCommonTest, FieldTrialPrefetchingSmallDB) {
  CreateTestFieldTrial("SpeculativeResourcePrefetching",
                       "Prefetching=Enabled:SmallDB=Enabled");
  ResourcePrefetchPredictorConfig config;
  EXPECT_TRUE(IsSpeculativeResourcePrefetchingEnabled(profile_.get(), &config));
  TestIsPrefetchEnabled(config);
  EXPECT_FALSE(config.IsLowConfidenceForTest());
  EXPECT_FALSE(config.IsHighConfidenceForTest());
  EXPECT_FALSE(config.IsMoreResourcesEnabledForTest());
  EXPECT_TRUE(config.IsSmallDBEnabledForTest());
}

TEST_F(ResourcePrefetchCommonTest, FieldTrialPrefetchingSmallDBLowConfidence) {
  CreateTestFieldTrial("SpeculativeResourcePrefetching",
                       "Prefetching=Enabled:SmallDB=Enabled:Confidence=Low");
  ResourcePrefetchPredictorConfig config;
  EXPECT_TRUE(IsSpeculativeResourcePrefetchingEnabled(profile_.get(), &config));
  TestIsPrefetchEnabled(config);
  EXPECT_TRUE(config.IsLowConfidenceForTest());
  EXPECT_FALSE(config.IsHighConfidenceForTest());
  EXPECT_FALSE(config.IsMoreResourcesEnabledForTest());
  EXPECT_TRUE(config.IsSmallDBEnabledForTest());
}

TEST_F(ResourcePrefetchCommonTest, FieldTrialPrefetchingSmallDBHighConfidence) {
  CreateTestFieldTrial("SpeculativeResourcePrefetching",
                       "Prefetching=Enabled:SmallDB=Enabled:Confidence=High");
  ResourcePrefetchPredictorConfig config;
  EXPECT_TRUE(IsSpeculativeResourcePrefetchingEnabled(profile_.get(), &config));
  TestIsPrefetchEnabled(config);
  EXPECT_FALSE(config.IsLowConfidenceForTest());
  EXPECT_TRUE(config.IsHighConfidenceForTest());
  EXPECT_FALSE(config.IsMoreResourcesEnabledForTest());
  EXPECT_TRUE(config.IsSmallDBEnabledForTest());
}

// Verifies whether prefetching in the field trial is disabled according to
// the network type. But learning should not be disabled by network.
TEST_F(ResourcePrefetchCommonTest, FieldTrialPrefetchingDisabledByNetwork) {
  CreateTestFieldTrial("SpeculativeResourcePrefetching",
                       "Prefetching=Enabled");
  ResourcePrefetchPredictorConfig config;
  EXPECT_TRUE(IsSpeculativeResourcePrefetchingEnabled(profile_.get(), &config));
  TestIsPrefetchEnabled(config);

  // Set preference to WIFI_ONLY: prefetch when not on cellular.
  SetPreference(NetworkPredictionOptions::NETWORK_PREDICTION_WIFI_ONLY);
  {
    scoped_ptr<NetworkChangeNotifier> mock(new MockNetworkChangeNotifierWIFI);
    TestIsPrefetchEnabled(config);
  }
  {
    scoped_ptr<NetworkChangeNotifier> mock(new MockNetworkChangeNotifier4G);
    TestIsPrefetchLearning(config);
  }

  // Set preference to ALWAYS: always prefetch.
  SetPreference(NetworkPredictionOptions::NETWORK_PREDICTION_ALWAYS);
  {
    scoped_ptr<NetworkChangeNotifier> mock(new MockNetworkChangeNotifierWIFI);
    TestIsPrefetchEnabled(config);
  }
  {
    scoped_ptr<NetworkChangeNotifier> mock(new MockNetworkChangeNotifier4G);
    TestIsPrefetchEnabled(config);
  }

  // Set preference to NEVER: never prefetch.
  SetPreference(NetworkPredictionOptions::NETWORK_PREDICTION_NEVER);
  {
    scoped_ptr<NetworkChangeNotifier> mock(new MockNetworkChangeNotifierWIFI);
    TestIsPrefetchLearning(config);
  }
  {
    scoped_ptr<NetworkChangeNotifier> mock(new MockNetworkChangeNotifier4G);
    TestIsPrefetchLearning(config);
  }
}

}  // namespace predictors
