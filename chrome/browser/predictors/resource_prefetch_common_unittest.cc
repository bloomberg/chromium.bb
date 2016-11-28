// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/browser/predictors/resource_prefetch_common.h"
#include "chrome/browser/predictors/resource_prefetch_predictor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
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

}  // namespace

namespace predictors {

class ResourcePrefetchCommonTest : public testing::Test {
 public:
  ResourcePrefetchCommonTest();

  void SetPreference(NetworkPredictionOptions value) {
    profile_->GetPrefs()->SetInteger(prefs::kNetworkPredictionOptions, value);
  }

  void SetCommandLineValue(const std::string& value) {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kSpeculativeResourcePrefetching, value);
  }

  void TestIsPrefetchEnabled(const ResourcePrefetchPredictorConfig& config) {
    EXPECT_TRUE(config.IsLearningEnabled());
    EXPECT_TRUE(config.IsPrefetchingEnabled(profile_.get()));
    EXPECT_TRUE(config.IsURLLearningEnabled());
    EXPECT_TRUE(config.IsHostLearningEnabled());
    EXPECT_TRUE(config.IsURLPrefetchingEnabled(profile_.get()));
    EXPECT_TRUE(config.IsHostPrefetchingEnabled(profile_.get()));
  }

  void TestIsPrefetchLearning(const ResourcePrefetchPredictorConfig& config) {
    EXPECT_TRUE(config.IsLearningEnabled());
    EXPECT_FALSE(config.IsPrefetchingEnabled(profile_.get()));
    EXPECT_TRUE(config.IsURLLearningEnabled());
    EXPECT_TRUE(config.IsHostLearningEnabled());
    EXPECT_FALSE(config.IsURLPrefetchingEnabled(profile_.get()));
    EXPECT_FALSE(config.IsHostPrefetchingEnabled(profile_.get()));
  }

  void TestIsDefaultExtraConfig(const ResourcePrefetchPredictorConfig& config) {
    EXPECT_FALSE(config.IsLowConfidenceForTest());
    EXPECT_FALSE(config.IsHighConfidenceForTest());
    EXPECT_FALSE(config.IsMoreResourcesEnabledForTest());
    EXPECT_FALSE(config.IsSmallDBEnabledForTest());
  }

 protected:
  base::MessageLoop loop_;
  content::TestBrowserThread ui_thread_;
  std::unique_ptr<TestingProfile> profile_;
};

ResourcePrefetchCommonTest::ResourcePrefetchCommonTest()
    : loop_(base::MessageLoop::TYPE_DEFAULT),
      ui_thread_(content::BrowserThread::UI, &loop_),
      profile_(new TestingProfile()) { }

TEST_F(ResourcePrefetchCommonTest, IsDisabledByDefault) {
  ResourcePrefetchPredictorConfig config;
  EXPECT_FALSE(
      IsSpeculativeResourcePrefetchingEnabled(profile_.get(), &config));

  EXPECT_FALSE(config.IsLearningEnabled());
  EXPECT_FALSE(config.IsPrefetchingEnabled(profile_.get()));
  EXPECT_FALSE(config.IsURLLearningEnabled());
  EXPECT_FALSE(config.IsHostLearningEnabled());
  EXPECT_FALSE(config.IsURLPrefetchingEnabled(profile_.get()));
  EXPECT_FALSE(config.IsHostPrefetchingEnabled(profile_.get()));

  TestIsDefaultExtraConfig(config);
}

TEST_F(ResourcePrefetchCommonTest, EnableLearning) {
  SetCommandLineValue("learning");
  ResourcePrefetchPredictorConfig config;
  EXPECT_TRUE(IsSpeculativeResourcePrefetchingEnabled(profile_.get(), &config));
  TestIsPrefetchLearning(config);
  TestIsDefaultExtraConfig(config);
}

TEST_F(ResourcePrefetchCommonTest, EnablePrefetch) {
  SetCommandLineValue("enabled");
  ResourcePrefetchPredictorConfig config;
  EXPECT_TRUE(IsSpeculativeResourcePrefetchingEnabled(profile_.get(), &config));
  TestIsPrefetchEnabled(config);
  TestIsDefaultExtraConfig(config);
}

// Verifies whether prefetching is disabled according to the network type. But
// learning should not be disabled by network.
TEST_F(ResourcePrefetchCommonTest, RespectsNetworkSettings) {
  SetCommandLineValue("enabled");
  ResourcePrefetchPredictorConfig config;
  EXPECT_TRUE(IsSpeculativeResourcePrefetchingEnabled(profile_.get(), &config));
  TestIsPrefetchEnabled(config);
  TestIsDefaultExtraConfig(config);

  // Set preference to WIFI_ONLY: prefetch when not on cellular.
  SetPreference(NetworkPredictionOptions::NETWORK_PREDICTION_WIFI_ONLY);
  {
    std::unique_ptr<NetworkChangeNotifier> mock(
        new MockNetworkChangeNotifierWIFI);
    TestIsPrefetchEnabled(config);
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
