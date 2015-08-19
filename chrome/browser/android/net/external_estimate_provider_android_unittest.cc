// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/net/external_estimate_provider_android.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Tests if the |ExternalEstimateProviderAndroid| APIs return false without the
// downstream implementation.
TEST(ExternalEstimateProviderAndroidTest, BasicsTest) {
  chrome::android::ExternalEstimateProviderAndroid external_estimate_provider;

  base::TimeDelta rtt;
  EXPECT_FALSE(external_estimate_provider.GetRTT(&rtt));

  int32_t kbps;
  EXPECT_FALSE(external_estimate_provider.GetDownstreamThroughputKbps(&kbps));
  EXPECT_FALSE(external_estimate_provider.GetUpstreamThroughputKbps(&kbps));

  base::TimeDelta time_since_last_update;
  EXPECT_FALSE(external_estimate_provider.GetTimeSinceLastUpdate(
      &time_since_last_update));
}

}  // namespace
