// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/scheduler_utils.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace notifications {
namespace {

// Verifies we can get the correct time stamp at certain hour in yesterday.
TEST(SchedulerUtilsTest, ToLocalYesterdayHour) {
  base::Time today, yesterday, expected;

  // Retrieve a timestamp of yesterday at 6am.
  EXPECT_TRUE(base::Time::FromString("10/15/07 12:45:12 PM", &today));
  EXPECT_TRUE(ToLocalYesterdayHour(6, today, &yesterday));
  EXPECT_TRUE(base::Time::FromString("10/14/07 06:00:00 AM", &expected));
  EXPECT_EQ(expected, yesterday);

  // Test an edge case, that the time is 0 am of a Monday.
  EXPECT_TRUE(base::Time::FromString("03/25/19 00:00:00 AM", &today));
  EXPECT_TRUE(ToLocalYesterdayHour(0, today, &yesterday));
  EXPECT_TRUE(base::Time::FromString("03/24/19 00:00:00 AM", &expected));
  EXPECT_EQ(expected, yesterday);
}

}  // namespace
}  // namespace notifications
