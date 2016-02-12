// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/build_time.h"
#include "base/generated_build_date.h"
#include "base/time/time.h"

#include "testing/gtest/include/gtest/gtest.h"

TEST(BuildTime, DateLooksValid) {
  char build_date[] = BUILD_DATE;

  EXPECT_EQ(11u, strlen(build_date));
  EXPECT_EQ(' ', build_date[3]);
  EXPECT_EQ(' ', build_date[6]);
}

TEST(BuildTime, TimeLooksValid) {
  char build_time[] = "00:00:00";

  EXPECT_EQ(8u, strlen(build_time));
  EXPECT_EQ(':', build_time[2]);
  EXPECT_EQ(':', build_time[5]);
}

TEST(BuildTime, InThePast) {
  EXPECT_TRUE(base::GetBuildTime() < base::Time::Now());
  EXPECT_TRUE(base::GetBuildTime() < base::Time::NowFromSystemTime());
}
