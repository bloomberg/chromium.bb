// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time/time.h"
#include "net/der/encode_values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace der {
namespace test {

TEST(EncodeValuesTest, EncodeTimeAsGeneralizedTime) {
  // Fri, 24 Jun 2016 17:04:54 GMT
  base::Time time =
      base::Time::UnixEpoch() + base::TimeDelta::FromSeconds(1466787894);
  GeneralizedTime generalized_time;
  ASSERT_TRUE(EncodeTimeAsGeneralizedTime(time, &generalized_time));
  EXPECT_EQ(2016, generalized_time.year);
  EXPECT_EQ(6, generalized_time.month);
  EXPECT_EQ(24, generalized_time.day);
  EXPECT_EQ(17, generalized_time.hours);
  EXPECT_EQ(4, generalized_time.minutes);
  EXPECT_EQ(54, generalized_time.seconds);
}

TEST(EncodeValuesTest, EncodeTimeFromBeforeWindows) {
  // 1 Jan 1570 00:00:00 GMT
  base::Time time =
      base::Time::UnixEpoch() - base::TimeDelta::FromSeconds(12622780800UL);
  GeneralizedTime generalized_time;
#if defined(OS_WIN)
  EXPECT_FALSE(EncodeTimeAsGeneralizedTime(time, &generalized_time));
#else
  ASSERT_TRUE(EncodeTimeAsGeneralizedTime(time, &generalized_time));
  EXPECT_EQ(1570, generalized_time.year);
  EXPECT_EQ(1, generalized_time.month);
  EXPECT_EQ(1, generalized_time.day);
  EXPECT_EQ(0, generalized_time.hours);
  EXPECT_EQ(0, generalized_time.minutes);
  EXPECT_EQ(0, generalized_time.seconds);
#endif
}

TEST(EncodeValuesTest, EncodeTimeAfterTimeTMax) {
  // 1 Jan 2039 00:00:00 GMT
  base::Time time =
      base::Time::UnixEpoch() + base::TimeDelta::FromSeconds(2177452800UL);
  GeneralizedTime generalized_time;
  ASSERT_TRUE(EncodeTimeAsGeneralizedTime(time, &generalized_time));
  EXPECT_EQ(2039, generalized_time.year);
  EXPECT_EQ(1, generalized_time.month);
  EXPECT_EQ(1, generalized_time.day);
  EXPECT_EQ(0, generalized_time.hours);
  EXPECT_EQ(0, generalized_time.minutes);
  EXPECT_EQ(0, generalized_time.seconds);
}

}  // namespace test

}  // namespace der

}  // namespace net
