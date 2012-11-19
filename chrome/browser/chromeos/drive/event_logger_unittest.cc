// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/event_logger.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace drive {

TEST(DriveEventLoggerTest, BasicLogging) {
  EventLogger logger(3);  // At most 3 events are kept.
  EXPECT_EQ(0U, logger.history().size());

  logger.Log("first");
  logger.Log("second");
  logger.Log("third");

  // Events are recorded in the chronological order with sequential IDs.
  ASSERT_EQ(3U, logger.history().size());
  EXPECT_EQ(0, logger.history()[0].id);
  EXPECT_EQ(std::string("first"), logger.history()[0].what);
  EXPECT_EQ(1, logger.history()[1].id);
  EXPECT_EQ(std::string("second"), logger.history()[1].what);
  EXPECT_EQ(2, logger.history()[2].id);
  EXPECT_EQ(std::string("third"), logger.history()[2].what);

  logger.Log("fourth");
  // It does not log events beyond the specified.
  ASSERT_EQ(3U, logger.history().size());
  // The oldest events is pushed out.
  EXPECT_EQ(1, logger.history()[0].id);
  EXPECT_EQ(std::string("second"), logger.history()[0].what);
  EXPECT_EQ(2, logger.history()[1].id);
  EXPECT_EQ(std::string("third"), logger.history()[1].what);
  EXPECT_EQ(3, logger.history()[2].id);
  EXPECT_EQ(std::string("fourth"), logger.history()[2].what);
}

}   // namespace drive
