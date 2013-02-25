// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_clock.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace test {

TEST(QuicClockTest, Now) {
  QuicClock clock;

  QuicTime start(base::TimeTicks::Now());
  QuicTime now = clock.ApproximateNow();
  QuicTime end(base::TimeTicks::Now());

  EXPECT_LE(start, now);
  EXPECT_LE(now, end);
}

TEST(QuicClockTest, NowAsDeltaSinceUnixEpoch) {
  QuicClock clock;

  QuicTime::Delta start(base::Time::Now() - base::Time::UnixEpoch());
  QuicTime::Delta now = clock.NowAsDeltaSinceUnixEpoch();
  QuicTime::Delta end(base::Time::Now() - base::Time::UnixEpoch());

  EXPECT_LE(start, now);
  EXPECT_LE(now, end);
}

}  // namespace test
}  // namespace net
