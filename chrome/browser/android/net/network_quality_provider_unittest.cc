// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/net/network_quality_provider.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Tests if the |NetworkQualityProvider| APIs return false without the
// downstream implementation.
TEST(NetworkQualityProviderTest, BasicsTest) {
  chrome::android::NetworkQualityProvider network_quality_provider;

  base::TimeDelta rtt;
  EXPECT_FALSE(network_quality_provider.GetRTT(&rtt));

  int32_t kbps;
  EXPECT_FALSE(network_quality_provider.GetDownstreamThroughputKbps(&kbps));
  EXPECT_FALSE(network_quality_provider.GetUpstreamThroughputKbps(&kbps));

  base::TimeDelta time_since_last_update;
  EXPECT_FALSE(
      network_quality_provider.GetTimeSinceLastUpdate(&time_since_last_update));
}

}  // namespace
