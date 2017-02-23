// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/stability_metrics_provider.h"

#include "components/metrics/proto/system_profile.pb.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

class StabilityMetricsProviderTest : public testing::Test {
 public:
  StabilityMetricsProviderTest() {
    StabilityMetricsProvider::RegisterPrefs(prefs_.registry());
  }

  ~StabilityMetricsProviderTest() override {}

 protected:
  TestingPrefServiceSimple prefs_;

 private:
  DISALLOW_COPY_AND_ASSIGN(StabilityMetricsProviderTest);
};

TEST_F(StabilityMetricsProviderTest, ProvideStabilityMetrics) {
  StabilityMetricsProvider stability_provider(&prefs_);
  MetricsProvider* provider = &stability_provider;
  SystemProfileProto system_profile;
  provider->ProvideStabilityMetrics(&system_profile);

  const SystemProfileProto_Stability& stability = system_profile.stability();
  // Initial log metrics: only expected if non-zero.
  EXPECT_FALSE(stability.has_launch_count());
  EXPECT_FALSE(stability.has_crash_count());
  EXPECT_FALSE(stability.has_incomplete_shutdown_count());
  EXPECT_FALSE(stability.has_breakpad_registration_success_count());
  EXPECT_FALSE(stability.has_breakpad_registration_failure_count());
  EXPECT_FALSE(stability.has_debugger_present_count());
  EXPECT_FALSE(stability.has_debugger_not_present_count());
}

TEST_F(StabilityMetricsProviderTest, RecordStabilityMetrics) {
  {
    StabilityMetricsProvider recorder(&prefs_);
    recorder.LogLaunch();
    recorder.LogCrash();
    recorder.MarkSessionEndCompleted(false);
    recorder.CheckLastSessionEndCompleted();
    recorder.RecordBreakpadRegistration(true);
    recorder.RecordBreakpadRegistration(false);
    recorder.RecordBreakpadHasDebugger(true);
    recorder.RecordBreakpadHasDebugger(false);
  }

  {
    StabilityMetricsProvider stability_provider(&prefs_);
    MetricsProvider* provider = &stability_provider;
    SystemProfileProto system_profile;
    provider->ProvideStabilityMetrics(&system_profile);

    const SystemProfileProto_Stability& stability = system_profile.stability();
    // Initial log metrics: only expected if non-zero.
    EXPECT_EQ(1, stability.launch_count());
    EXPECT_EQ(1, stability.crash_count());
    EXPECT_EQ(1, stability.incomplete_shutdown_count());
    EXPECT_EQ(1, stability.breakpad_registration_success_count());
    EXPECT_EQ(1, stability.breakpad_registration_failure_count());
    EXPECT_EQ(1, stability.debugger_present_count());
    EXPECT_EQ(1, stability.debugger_not_present_count());
  }
}

}  // namespace metrics
