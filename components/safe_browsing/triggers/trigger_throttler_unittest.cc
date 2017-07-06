// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/triggers/trigger_throttler.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {
namespace {

TEST(TriggerThrottler, SecurityInterstitialsHaveUnlimitedQuota) {
  // Make sure that security interstitials never run out of quota.
  TriggerThrottler throttler;
  for (int i = 0; i < 1000; ++i) {
    throttler.TriggerFired(TriggerType::SECURITY_INTERSTITIAL);
    EXPECT_TRUE(throttler.TriggerCanFire(TriggerType::SECURITY_INTERSTITIAL));
  }
}

}  // namespace
}  // namespace safe_browsing
