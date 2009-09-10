// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/base/time.h"

namespace notifier {

TEST_NOTIFIER_F(TimeTest);

TEST_F(TimeTest, ParseRFC822DateTime) {
  struct tm t = {0};

  EXPECT_TRUE(ParseRFC822DateTime("Mon, 16 May 2005 15:44:18 -0700",
                                  &t, false));
  EXPECT_EQ(t.tm_year, 2005 - 1900);
  EXPECT_EQ(t.tm_mon, 4);
  EXPECT_EQ(t.tm_mday, 16);
  EXPECT_EQ(t.tm_hour, 22);
  EXPECT_EQ(t.tm_min, 44);
  EXPECT_EQ(t.tm_sec, 18);

  EXPECT_TRUE(ParseRFC822DateTime("Mon, 16 May 2005 15:44:18 -0700", &t, true));
  EXPECT_EQ(t.tm_year, 2005 - 1900);
  EXPECT_EQ(t.tm_mon, 4);
  EXPECT_EQ(t.tm_mday, 16);
  EXPECT_TRUE(t.tm_hour == 15 || t.tm_hour == 14);  // daylight saving time
  EXPECT_EQ(t.tm_min, 44);
  EXPECT_EQ(t.tm_sec , 18);

  EXPECT_TRUE(ParseRFC822DateTime("Tue, 17 May 2005 02:56:18 +0400",
                                  &t, false));
  EXPECT_EQ(t.tm_year, 2005 - 1900);
  EXPECT_EQ(t.tm_mon, 4);
  EXPECT_EQ(t.tm_mday, 16);
  EXPECT_EQ(t.tm_hour, 22);
  EXPECT_EQ(t.tm_min, 56);
  EXPECT_EQ(t.tm_sec , 18);

  EXPECT_TRUE(ParseRFC822DateTime("Tue, 17 May 2005 02:56:18 +0400", &t, true));
  EXPECT_EQ(t.tm_year, 2005 - 1900);
  EXPECT_EQ(t.tm_mon, 4);
  EXPECT_EQ(t.tm_mday, 16);
  EXPECT_TRUE(t.tm_hour == 15 || t.tm_hour == 14);  // daylight saving time
  EXPECT_EQ(t.tm_min, 56);
  EXPECT_EQ(t.tm_sec, 18);
}

TEST_F(TimeTest, ParseStringToTimeSpan) {
  time64 time_span = 0;

  EXPECT_TRUE(ParseStringToTimeSpan("0:0:4", &time_span));
  EXPECT_EQ(time_span, 4 * kSecsTo100ns);

  EXPECT_TRUE(ParseStringToTimeSpan("0:3:4", &time_span));
  EXPECT_EQ(time_span, (3 * 60 + 4) * kSecsTo100ns);

  EXPECT_TRUE(ParseStringToTimeSpan("2:3:4", &time_span));
  EXPECT_EQ(time_span, (2 * 3600 + 3 * 60 + 4) * kSecsTo100ns);

  EXPECT_TRUE(ParseStringToTimeSpan("1.2:3:4", &time_span));
  EXPECT_EQ(time_span, (1 * 86400 + 2 * 60 * 60 + 3 * 60 + 4) * kSecsTo100ns);

  EXPECT_FALSE(ParseStringToTimeSpan("2:invalid:4", &time_span));
}

TEST_F(TimeTest, UseLocalTimeAsString) {
  // Just call it to ensure that it doesn't assert.
  GetLocalTimeAsString();
}

}  // namespace notifier
