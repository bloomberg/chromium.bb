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

// Checks that the unserialized pending profile is encoded in the session data.
TEST_F(CallStackProfileMetricsProviderTest,
       ProvideCurrentSessionDataUnserialized) {
  CallStackProfileMetricsProvider provider;
  provider.OnRecordingEnabled();
  SampledProfile profile;
  profile.set_trigger_event(SampledProfile::PERIODIC_COLLECTION);
  CallStackProfileMetricsProvider::ReceiveProfile(base::TimeTicks::Now(),
                                                  profile);
  ChromeUserMetricsExtension uma_proto;
  provider.ProvideCurrentSessionData(&uma_proto);
  ASSERT_EQ(1, uma_proto.sampled_profile().size());
  EXPECT_EQ(SampledProfile::PERIODIC_COLLECTION,
            uma_proto.sampled_profile(0).trigger_event());
}

// Checks that the serialized pending profile is encoded in the session data.
TEST_F(CallStackProfileMetricsProviderTest,
       ProvideCurrentSessionDataSerialized) {
  CallStackProfileMetricsProvider provider;
  provider.OnRecordingEnabled();
  std::string contents;
  {
    SampledProfile profile;
    profile.set_trigger_event(SampledProfile::PERIODIC_COLLECTION);
    profile.SerializeToString(&contents);
  }
  CallStackProfileMetricsProvider::ReceiveSerializedProfile(
      base::TimeTicks::Now(), contents);
  ChromeUserMetricsExtension uma_proto;
  provider.ProvideCurrentSessionData(&uma_proto);
  ASSERT_EQ(1, uma_proto.sampled_profile().size());
  EXPECT_EQ(SampledProfile::PERIODIC_COLLECTION,
            uma_proto.sampled_profile(0).trigger_event());
}

// Checks that the pending profile is provided to ProvideCurrentSessionData
// when collected before CallStackProfileMetricsProvider is instantiated.
TEST_F(CallStackProfileMetricsProviderTest,
       ProfileProvidedWhenCollectedBeforeInstantiation) {
  CallStackProfileMetricsProvider::ReceiveProfile(base::TimeTicks::Now(),
                                                  SampledProfile());
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
  CallStackProfileMetricsProvider::ReceiveProfile(base::TimeTicks::Now(),
                                                  SampledProfile());
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
  base::TimeTicks profile_start_time = base::TimeTicks::Now();
  provider.OnRecordingDisabled();
  CallStackProfileMetricsProvider::ReceiveProfile(profile_start_time,
                                                  SampledProfile());
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
  base::TimeTicks profile_start_time = base::TimeTicks::Now();
  provider.OnRecordingDisabled();
  provider.OnRecordingEnabled();
  CallStackProfileMetricsProvider::ReceiveProfile(profile_start_time,
                                                  SampledProfile());
  ChromeUserMetricsExtension uma_proto;
  provider.ProvideCurrentSessionData(&uma_proto);
  EXPECT_EQ(0, uma_proto.sampled_profile_size());
}

// Checks that the pending profile is provided to ProvideCurrentSessionData
// if recording is disabled, but then enabled while profiling.
TEST_F(CallStackProfileMetricsProviderTest,
       ProfileNotProvidedAfterChangeFromDisabled) {
  CallStackProfileMetricsProvider provider;
  provider.OnRecordingDisabled();
  base::TimeTicks profile_start_time = base::TimeTicks::Now();
  provider.OnRecordingEnabled();
  CallStackProfileMetricsProvider::ReceiveProfile(profile_start_time,
                                                  SampledProfile());
  ChromeUserMetricsExtension uma_proto;
  provider.ProvideCurrentSessionData(&uma_proto);
  EXPECT_EQ(0, uma_proto.sampled_profile_size());
}

}  // namespace metrics
