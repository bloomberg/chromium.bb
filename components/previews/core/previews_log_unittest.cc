// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/core/previews_log.h"

#include <vector>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace previews {

namespace {

const char kPreviewsNavigationEventType[] = "Navigation";
const size_t kMaximumMessageLogs = 10;

}  // namespace

class PreviewsLoggerTest : public testing::Test {
 public:
  PreviewsLoggerTest() {}

  ~PreviewsLoggerTest() override {}

  void SetUp() override { logger_ = base::MakeUnique<PreviewsLogger>(); }

 protected:
  std::unique_ptr<PreviewsLogger> logger_;
};

TEST_F(PreviewsLoggerTest, AddLogMessageToLogger) {
  size_t expected_size = 0;
  EXPECT_EQ(expected_size, logger_->log_messages().size());

  const PreviewsLogger::MessageLog expected_messages[] = {
      PreviewsLogger::MessageLog("Event_a", "Some description a",
                                 GURL("http://www.url_a.com/url_a"),
                                 base::Time::Now()),
      PreviewsLogger::MessageLog("Event_b", "Some description b",
                                 GURL("http://www.url_b.com/url_b"),
                                 base::Time::Now()),
      PreviewsLogger::MessageLog("Event_c", "Some description c",
                                 GURL("http://www.url_c.com/url_c"),
                                 base::Time::Now()),
  };

  for (auto message : expected_messages) {
    logger_->LogMessage(message.event_type, message.event_description,
                        message.url, message.time);
  }

  std::vector<PreviewsLogger::MessageLog> actual = logger_->log_messages();

  expected_size = 3;
  EXPECT_EQ(expected_size, actual.size());

  for (size_t i = 0; i < actual.size(); i++) {
    EXPECT_EQ(expected_messages[i].event_type, actual[i].event_type);
    EXPECT_EQ(expected_messages[i].event_description,
              actual[i].event_description);
    EXPECT_EQ(expected_messages[i].time, actual[i].time);
    EXPECT_EQ(expected_messages[i].url, actual[i].url);
  }
}

TEST_F(PreviewsLoggerTest, LogPreviewNavigationLogMessage) {
  const base::Time time = base::Time::Now();

  PreviewsType type_a = PreviewsType::OFFLINE;
  PreviewsType type_b = PreviewsType::LOFI;
  const GURL url_a("http://www.url_a.com/url_a");
  const GURL url_b("http://www.url_b.com/url_b");

  PreviewsLogger::PreviewNavigation navigation_a(url_a, type_a,
                                                 true /* opt_out */, time);
  PreviewsLogger::PreviewNavigation navigation_b(url_b, type_b,
                                                 false /* opt_out */, time);

  logger_->LogPreviewNavigation(navigation_a);
  logger_->LogPreviewNavigation(navigation_b);

  std::vector<PreviewsLogger::MessageLog> actual = logger_->log_messages();

  const size_t expected_size = 2;
  EXPECT_EQ(expected_size, actual.size());

  std::string expected_description_a =
      "Offline preview navigation - user opt_out: True";
  EXPECT_EQ(kPreviewsNavigationEventType, actual[0].event_type);
  EXPECT_EQ(expected_description_a, actual[0].event_description);
  EXPECT_EQ(url_a, actual[0].url);
  EXPECT_EQ(time, actual[0].time);

  std::string expected_description_b =
      "LoFi preview navigation - user opt_out: False";
  EXPECT_EQ(kPreviewsNavigationEventType, actual[1].event_type);
  EXPECT_EQ(expected_description_b, actual[1].event_description);
  EXPECT_EQ(url_b, actual[1].url);
  EXPECT_EQ(time, actual[1].time);
}

TEST_F(PreviewsLoggerTest, PreviewsLoggerOnlyKeepsCertainNumberOfMessageLogs) {
  const std::string event_type = "Event";
  const std::string event_description = "Some description";
  const base::Time time = base::Time::Now();
  const GURL url("http://www.url_.com/url_");

  for (size_t i = 0; i < 2 * kMaximumMessageLogs; i++) {
    logger_->LogMessage(event_type, event_description, url, time);
  }

  EXPECT_EQ(kMaximumMessageLogs, logger_->log_messages().size());
}

}  // namespace previews
