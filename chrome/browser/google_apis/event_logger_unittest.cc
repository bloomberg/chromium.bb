// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/event_logger.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace google_apis {

TEST(EventLoggerTest, BasicLogging) {
  EventLogger logger;
  logger.SetHistorySize(3);  // At most 3 events are kept.
  EXPECT_EQ(0U, logger.GetHistory().size());

  logger.Log("first");
  logger.Log("%dnd", 2);
  logger.Log("third");

  // Events are recorded in the chronological order with sequential IDs.
  std::vector<EventLogger::Event> history = logger.GetHistory();
  ASSERT_EQ(3U, history.size());
  EXPECT_EQ(0, history[0].id);
  EXPECT_EQ("first", history[0].what);
  EXPECT_EQ(1, history[1].id);
  EXPECT_EQ("2nd", history[1].what);
  EXPECT_EQ(2, history[2].id);
  EXPECT_EQ("third", history[2].what);

  logger.Log("fourth");
  // It does not log events beyond the specified.
  history = logger.GetHistory();
  ASSERT_EQ(3U, history.size());
  // The oldest events is pushed out.
  EXPECT_EQ(1, history[0].id);
  EXPECT_EQ("2nd", history[0].what);
  EXPECT_EQ(2, history[1].id);
  EXPECT_EQ("third", history[1].what);
  EXPECT_EQ(3, history[2].id);
  EXPECT_EQ("fourth", history[2].what);
}

}   // namespace google_apis
