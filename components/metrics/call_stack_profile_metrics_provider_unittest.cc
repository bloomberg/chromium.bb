// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/call_stack_profile_metrics_provider.h"

#include <utility>

#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

namespace metrics {

// This test fixture enables the feature that
// CallStackProfileMetricsProvider depends on to report a profile.
class CallStackProfileMetricsProviderTest : public testing::Test {
 public:
  CallStackProfileMetricsProviderTest() {
    scoped_feature_list_.InitAndEnableFeature(TestState::kEnableReporting);
    TestState::ResetStaticStateForTesting();
  }

  ~CallStackProfileMetricsProviderTest() override {}

  // Utility function to append a profile to the metrics provider.
  void AppendProfile(SampledProfile proto) {
    CallStackProfileMetricsProvider::GetProfilerCallbackForBrowserProcess().Run(
        std::move(proto));
  }

 private:
  // Exposes the feature from the CallStackProfileMetricsProvider.
  class TestState : public CallStackProfileMetricsProvider {
   public:
    using CallStackProfileMetricsProvider::kEnableReporting;
    using CallStackProfileMetricsProvider::ResetStaticStateForTesting;
  };

  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(CallStackProfileMetricsProviderTest);
};

// Checks that the pending profile is passed to ProvideCurrentSessionData.
TEST_F(CallStackProfileMetricsProviderTest, ProvideCurrentSessionData) {
  CallStackProfileMetricsProvider provider;
  provider.OnRecordingEnabled();
  AppendProfile(SampledProfile());
  ChromeUserMetricsExtension uma_proto;
  provider.ProvideCurrentSessionData(&uma_proto);
  ASSERT_EQ(1, uma_proto.sampled_profile().size());
}

// Checks that the pending profile is provided to ProvideCurrentSessionData
// when collected before CallStackProfileMetricsProvider is instantiated.
TEST_F(CallStackProfileMetricsProviderTest,
       ProfileProvidedWhenCollectedBeforeInstantiation) {
  AppendProfile(SampledProfile());
  CallStackProfileMetricsProvider provider;
  provider.OnRecordingEnabled();
  ChromeUserMetricsExtension uma_proto;
  provider.ProvideCurrentSessionData(&uma_proto);
  EXPECT_EQ(1, uma_proto.sampled_profile_size());
}

// Checks that the pending profile is not provided to ProvideCurrentSessionData
// while recording is disabled.
TEST_F(CallStackProfileMetricsProviderTest, ProfileNotProvidedWhileDisabled) {
  CallStackProfileMetricsProvider provider;
  provider.OnRecordingDisabled();
  AppendProfile(SampledProfile());
  ChromeUserMetricsExtension uma_proto;
  provider.ProvideCurrentSessionData(&uma_proto);
  EXPECT_EQ(0, uma_proto.sampled_profile_size());
}

// Checks that the pending profile is not provided to ProvideCurrentSessionData
// if recording is disabled while profiling.
TEST_F(CallStackProfileMetricsProviderTest,
       ProfileNotProvidedAfterChangeToDisabled) {
  CallStackProfileMetricsProvider provider;
  provider.OnRecordingEnabled();
  CallStackProfileBuilder::CompletedCallback callback =
      CallStackProfileMetricsProvider::GetProfilerCallbackForBrowserProcess();
  provider.OnRecordingDisabled();
  callback.Run(SampledProfile());
  ChromeUserMetricsExtension uma_proto;
  provider.ProvideCurrentSessionData(&uma_proto);
  EXPECT_EQ(0, uma_proto.sampled_profile_size());
}

// Checks that the pending profile is not provided to ProvideCurrentSessionData
// if recording is enabled, but then disabled and reenabled while profiling.
TEST_F(CallStackProfileMetricsProviderTest,
       ProfileNotProvidedAfterChangeToDisabledThenEnabled) {
  CallStackProfileMetricsProvider provider;
  provider.OnRecordingEnabled();
  CallStackProfileBuilder::CompletedCallback callback =
      CallStackProfileMetricsProvider::GetProfilerCallbackForBrowserProcess();
  provider.OnRecordingDisabled();
  provider.OnRecordingEnabled();
  callback.Run(SampledProfile());
  ChromeUserMetricsExtension uma_proto;
  provider.ProvideCurrentSessionData(&uma_proto);
  EXPECT_EQ(0, uma_proto.sampled_profile_size());
}

// Checks that the pending profile is not provided to ProvideCurrentSessionData
// if recording is disabled, but then enabled while profiling.
TEST_F(CallStackProfileMetricsProviderTest,
       ProfileNotProvidedAfterChangeFromDisabled) {
  CallStackProfileMetricsProvider provider;
  provider.OnRecordingDisabled();
  CallStackProfileBuilder::CompletedCallback callback =
      CallStackProfileMetricsProvider::GetProfilerCallbackForBrowserProcess();
  provider.OnRecordingEnabled();
  callback.Run(SampledProfile());
  ChromeUserMetricsExtension uma_proto;
  provider.ProvideCurrentSessionData(&uma_proto);
  EXPECT_EQ(0, uma_proto.sampled_profile_size());
}

}  // namespace metrics
