// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/instant/promo_counter.h"
#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

typedef testing::Test PromoCounterTest;

// Makes sure ShouldShow returns false after the max number of days.
TEST_F(PromoCounterTest, MaxTimeElapsed) {
  TestingProfile profile;
  PromoCounter::RegisterUserPrefs(profile.GetPrefs(), "test");

  base::Time test_time(base::Time::Now());
  PromoCounter counter(&profile, "test", "test", 2, 2);
  ASSERT_TRUE(counter.ShouldShow(test_time));
  ASSERT_TRUE(counter.ShouldShow(test_time + base::TimeDelta::FromHours(2)));
  ASSERT_FALSE(counter.ShouldShow(test_time + base::TimeDelta::FromDays(4)));
}

// Makes sure ShouldShow returns false after max number of sessions encountered.
TEST_F(PromoCounterTest, MaxSessionsLapsed) {
  TestingProfile profile;
  PromoCounter::RegisterUserPrefs(profile.GetPrefs(), "test");

  base::Time test_time(base::Time::Now());
  {
    PromoCounter counter(&profile, "test", "test", 2, 2);
    ASSERT_TRUE(counter.ShouldShow(test_time));
  }

  {
    PromoCounter counter(&profile, "test", "test", 2, 2);
    ASSERT_TRUE(counter.ShouldShow(test_time));
  }

  {
    PromoCounter counter(&profile, "test", "test", 2, 2);
    ASSERT_FALSE(counter.ShouldShow(test_time));
  }
}

// Makes sure invoking Hide make ShouldShow return false.
TEST_F(PromoCounterTest, Hide) {
  TestingProfile profile;
  PromoCounter::RegisterUserPrefs(profile.GetPrefs(), "test");

  base::Time test_time(base::Time::Now());
  {
    PromoCounter counter(&profile, "test", "test", 2, 2);
    counter.Hide();
    ASSERT_FALSE(counter.ShouldShow(test_time));
  }

  // Recreate to make sure pref was correctly written.
  {
    PromoCounter counter(&profile, "test", "test", 2, 2);
    ASSERT_FALSE(counter.ShouldShow(test_time));
  }
}

// Same as Hide, but invokes ShouldShow first.
TEST_F(PromoCounterTest, Hide2) {
  TestingProfile profile;
  PromoCounter::RegisterUserPrefs(profile.GetPrefs(), "test");

  base::Time test_time(base::Time::Now());
  {
    PromoCounter counter(&profile, "test", "test", 2, 2);
    ASSERT_TRUE(counter.ShouldShow(test_time));
    counter.Hide();
    ASSERT_FALSE(counter.ShouldShow(test_time));
  }

  {
    PromoCounter counter(&profile, "test", "test", 2, 2);
    ASSERT_FALSE(counter.ShouldShow(test_time));
  }
}
