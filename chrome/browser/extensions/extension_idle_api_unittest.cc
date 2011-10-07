// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "chrome/browser/extensions/extension_idle_api.h"

TEST(ExtensionIdleApiTest, CacheTest) {
  double throttle_interval = ExtensionIdleCache::get_throttle_interval();
  int min_threshold = ExtensionIdleCache::get_min_threshold();
  double now = 10 * min_threshold;

  EXPECT_EQ(IDLE_STATE_UNKNOWN,
            ExtensionIdleCache::CalculateState(min_threshold, now));

  ExtensionIdleCache::Update(2 * min_threshold, IDLE_STATE_IDLE, now);

  EXPECT_EQ(IDLE_STATE_IDLE,
            ExtensionIdleCache::CalculateState(2 * min_threshold, now));
  EXPECT_EQ(IDLE_STATE_IDLE,
      ExtensionIdleCache::CalculateState(2 * min_threshold,
                                         now + 0.9 * throttle_interval));
  EXPECT_EQ(IDLE_STATE_IDLE,
      ExtensionIdleCache::CalculateState(min_threshold,
                                         now + 0.1 * throttle_interval));
  // Threshold exeeds idle interval boundries.
  EXPECT_EQ(IDLE_STATE_UNKNOWN,
      ExtensionIdleCache::CalculateState(2 * min_threshold + 1,
                                         now + 0.1 * throttle_interval));
  // It has been more than throttle interval since last query.
  EXPECT_EQ(IDLE_STATE_UNKNOWN,
      ExtensionIdleCache::CalculateState(min_threshold,
                                         now + 1.1 * throttle_interval));

  now += 10 * min_threshold;
  // Idle interval does not overlap with previous one.
  ExtensionIdleCache::Update(5 * min_threshold, IDLE_STATE_IDLE, now);
  EXPECT_EQ(IDLE_STATE_UNKNOWN,
            ExtensionIdleCache::CalculateState(7 * min_threshold, now));

  now += min_threshold;
  // Idle interval overlaps with previous one.
  ExtensionIdleCache::Update(2 * min_threshold, IDLE_STATE_IDLE, now);
  // Threshold exeeds last idle interval boundaries, but does not exeed union of
  // two last (overlaping) idle intervals.
  EXPECT_EQ(IDLE_STATE_IDLE,
            ExtensionIdleCache::CalculateState(4 * min_threshold, now));

  now += 0.2 * throttle_interval;
  ExtensionIdleCache::Update(8 * min_threshold, IDLE_STATE_ACTIVE, now);
  EXPECT_EQ(IDLE_STATE_IDLE,
      ExtensionIdleCache::CalculateState(4 * min_threshold,
                                         now + 0.3 * throttle_interval));

  // If both idle and active conditions are satisfied, return ACTIVE (because
  // obviously ACTIVE was reported after last idle interval).
  ExtensionIdleCache::Update(3 * min_threshold, IDLE_STATE_ACTIVE, now);
  EXPECT_EQ(IDLE_STATE_ACTIVE,
      ExtensionIdleCache::CalculateState(4 * min_threshold,
                                         now + 0.3 * throttle_interval));

  now += 10 * min_threshold;
  ExtensionIdleCache::Update(8 * min_threshold, IDLE_STATE_ACTIVE, now);
  // Threshold does not exeed last active state, but the error is within
  // throttle interval.
  EXPECT_EQ(IDLE_STATE_ACTIVE,
      ExtensionIdleCache::CalculateState(8 * min_threshold,
                                         now + 0.3 * throttle_interval));
  // The error is not within throttle interval.
  EXPECT_EQ(IDLE_STATE_UNKNOWN,
      ExtensionIdleCache::CalculateState(8 * min_threshold,
                                         now + 1.1 * throttle_interval));

  // We report LOCKED iff it was last reported state was LOCKED and  it has
  // been less than throttle_interval since last query.
  now += 10 * min_threshold;
  ExtensionIdleCache::Update(4 * min_threshold, IDLE_STATE_LOCKED, now);
  EXPECT_EQ(IDLE_STATE_LOCKED,
      ExtensionIdleCache::CalculateState(2 * min_threshold,
                                         now + 0.3 * throttle_interval));
  // More than throttle_interval since last query.
  EXPECT_EQ(IDLE_STATE_UNKNOWN,
      ExtensionIdleCache::CalculateState(2 * min_threshold,
                                         now + 1.1 * throttle_interval));

  now += 0.2 * throttle_interval;
  ExtensionIdleCache::Update(4 * min_threshold, IDLE_STATE_ACTIVE, now);
  // Last reported query was ACTIVE.
  EXPECT_EQ(IDLE_STATE_UNKNOWN,
      ExtensionIdleCache::CalculateState(2 * min_threshold,
                                         now + 0.3 * throttle_interval));

  now += 0.2 * throttle_interval;
  ExtensionIdleCache::Update(2 * min_threshold, IDLE_STATE_LOCKED, now);
  EXPECT_EQ(IDLE_STATE_LOCKED,
      ExtensionIdleCache::CalculateState(5 * min_threshold,
                                         now + 0.3 * throttle_interval));

  now += 10 * min_threshold;
  ExtensionIdleCache::Update(4 * min_threshold, IDLE_STATE_LOCKED, now);

  now += 0.2 * throttle_interval;
  ExtensionIdleCache::Update(2 * min_threshold, IDLE_STATE_IDLE, now);

  // Last reported state was IDLE.
  EXPECT_EQ(IDLE_STATE_UNKNOWN,
      ExtensionIdleCache::CalculateState(3 * min_threshold,
                                         now + 0.3 * throttle_interval));

  now += min_threshold;
  ExtensionIdleCache::Update(2 * min_threshold, IDLE_STATE_LOCKED, now);

  now += 0.2 * throttle_interval;
  ExtensionIdleCache::Update(4 * min_threshold, IDLE_STATE_ACTIVE, now);

  // Last reported state was ACTIVE.
  EXPECT_EQ(IDLE_STATE_ACTIVE,
      ExtensionIdleCache::CalculateState(6 * min_threshold,
                                         now + 0.3 * throttle_interval));
}
