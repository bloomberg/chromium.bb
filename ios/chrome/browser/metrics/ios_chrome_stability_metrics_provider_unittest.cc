// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/metrics/ios_chrome_stability_metrics_provider.h"

#include "base/macros.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace {

class IOSChromeStabilityMetricsProviderTest : public PlatformTest {
 protected:
  IOSChromeStabilityMetricsProviderTest()
      : prefs_(new TestingPrefServiceSimple) {
    metrics::StabilityMetricsHelper::RegisterPrefs(prefs()->registry());
  }

  TestingPrefServiceSimple* prefs() { return prefs_.get(); }

 private:
  std::unique_ptr<TestingPrefServiceSimple> prefs_;

  DISALLOW_COPY_AND_ASSIGN(IOSChromeStabilityMetricsProviderTest);
};

}  // namespace

TEST_F(IOSChromeStabilityMetricsProviderTest, LogLoadStart) {
  IOSChromeStabilityMetricsProvider provider(prefs());

  // A load should not increment metrics if recording is disabled.
  provider.WebStateDidStartLoading(nullptr);

  metrics::SystemProfileProto system_profile;

  // Call ProvideStabilityMetrics to check that it will force pending tasks to
  // be executed immediately.
  provider.ProvideStabilityMetrics(&system_profile);

  EXPECT_EQ(0, system_profile.stability().page_load_count());

  // A load should increment metrics if recording is enabled.
  provider.OnRecordingEnabled();
  provider.WebStateDidStartLoading(nullptr);

  system_profile.Clear();
  provider.ProvideStabilityMetrics(&system_profile);

  EXPECT_EQ(1, system_profile.stability().page_load_count());
}

TEST_F(IOSChromeStabilityMetricsProviderTest, LogRendererCrash) {
  IOSChromeStabilityMetricsProvider provider(prefs());

  // A crash should not increment the renderer crash count if recording is
  // disabled.
  provider.LogRendererCrash();

  metrics::SystemProfileProto system_profile;

  // Call ProvideStabilityMetrics to check that it will force pending tasks to
  // be executed immediately.
  provider.ProvideStabilityMetrics(&system_profile);

  EXPECT_EQ(0, system_profile.stability().renderer_crash_count());
  EXPECT_EQ(0, system_profile.stability().renderer_failed_launch_count());
  EXPECT_EQ(0, system_profile.stability().extension_renderer_crash_count());

  // A crash should increment the renderer crash count if recording is
  // enabled.
  provider.OnRecordingEnabled();
  provider.LogRendererCrash();

  system_profile.Clear();
  provider.ProvideStabilityMetrics(&system_profile);

  EXPECT_EQ(1, system_profile.stability().renderer_crash_count());
  EXPECT_EQ(0, system_profile.stability().renderer_failed_launch_count());
  EXPECT_EQ(0, system_profile.stability().extension_renderer_crash_count());
}
