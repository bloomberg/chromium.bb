// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/core/previews_logger.h"

#include <vector>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "components/previews/core/previews_black_list.h"
#include "components/previews/core/previews_logger_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace previews {

namespace {

const char kPreviewsDecisionMadeEventType[] = "Decision";
const char kPreviewsNavigationEventType[] = "Navigation";
const size_t kMaximumNavigationLogs = 10;
const size_t kMaximumDecisionLogs = 25;

// Mock class to test correct MessageLog is passed back to the
// mojo::InterventionsInternalsPagePtr.
class TestPreviewsLoggerObserver : public PreviewsLoggerObserver {
 public:
  TestPreviewsLoggerObserver() {}

  ~TestPreviewsLoggerObserver() override {}

  // PreviewsLoggerObserver:
  void OnNewMessageLogAdded(
      const PreviewsLogger::MessageLog& message) override {
    message_ = base::MakeUnique<PreviewsLogger::MessageLog>(message);
    messages_.push_back(*message_);
  }

  // Expose the passed in MessageLog for testing.
  PreviewsLogger::MessageLog* message() const { return message_.get(); }

  // Expose the received MessageLogs for testing.
  const std::vector<PreviewsLogger::MessageLog>& messages() const {
    return messages_;
  };

 private:
  // Received messages.
  std::vector<PreviewsLogger::MessageLog> messages_;

  // The passed in MessageLog in OnNewMessageLogAdded.
  std::unique_ptr<PreviewsLogger::MessageLog> message_;
};

class PreviewsLoggerTest : public testing::Test {
 public:
  PreviewsLoggerTest() {}

  ~PreviewsLoggerTest() override {}

  void SetUp() override { logger_ = base::MakeUnique<PreviewsLogger>(); }

 protected:
  std::unique_ptr<PreviewsLogger> logger_;
};

TEST_F(PreviewsLoggerTest, LogPreviewDecisionMadeLogMessage) {
  const base::Time time = base::Time::Now();

  PreviewsType type_a = PreviewsType::OFFLINE;
  PreviewsType type_b = PreviewsType::LOFI;
  PreviewsEligibilityReason reason_a =
      PreviewsEligibilityReason::BLACKLIST_UNAVAILABLE;
  PreviewsEligibilityReason reason_b =
      PreviewsEligibilityReason::NETWORK_NOT_SLOW;
  const GURL url_a("http://www.url_a.com/url_a");
  const GURL url_b("http://www.url_b.com/url_b");

  TestPreviewsLoggerObserver observer;
  logger_->AddAndNotifyObserver(&observer);

  logger_->LogPreviewDecisionMade(reason_a, url_a, time, type_a);
  logger_->LogPreviewDecisionMade(reason_b, url_b, time, type_b);

  auto actual = observer.messages();
  const size_t expected_size = 2;
  EXPECT_EQ(expected_size, actual.size());

  std::string expected_description_a =
      "Offline preview decision made - Blacklist failed to be created";
  EXPECT_EQ(kPreviewsDecisionMadeEventType, actual[0].event_type);
  EXPECT_EQ(expected_description_a, actual[0].event_description);
  EXPECT_EQ(url_a, actual[0].url);
  EXPECT_EQ(time, actual[0].time);

  std::string expected_description_b =
      "LoFi preview decision made - Network not slow";
  EXPECT_EQ(kPreviewsDecisionMadeEventType, actual[1].event_type);
  EXPECT_EQ(expected_description_b, actual[1].event_description);
  EXPECT_EQ(url_b, actual[1].url);
  EXPECT_EQ(time, actual[1].time);
}

TEST_F(PreviewsLoggerTest, LogPreviewNavigationLogMessage) {
  const base::Time time = base::Time::Now();

  PreviewsType type_a = PreviewsType::OFFLINE;
  PreviewsType type_b = PreviewsType::LOFI;
  const GURL url_a("http://www.url_a.com/url_a");
  const GURL url_b("http://www.url_b.com/url_b");

  TestPreviewsLoggerObserver observer;
  logger_->AddAndNotifyObserver(&observer);

  logger_->LogPreviewNavigation(url_a, type_a, true /* opt_out */, time);
  logger_->LogPreviewNavigation(url_b, type_b, false /* opt_out */, time);

  auto actual = observer.messages();

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

TEST_F(PreviewsLoggerTest, PreviewsLoggerOnlyKeepsCertainNumberOfDecisionLogs) {
  PreviewsType type = PreviewsType::OFFLINE;
  PreviewsEligibilityReason reason =
      PreviewsEligibilityReason::BLACKLIST_UNAVAILABLE;
  const base::Time time = base::Time::Now();
  const GURL url("http://www.url_.com/url_");

  for (size_t i = 0; i < 2 * kMaximumDecisionLogs; i++) {
    logger_->LogPreviewDecisionMade(reason, url, time, type);
  }

  TestPreviewsLoggerObserver observer;
  logger_->AddAndNotifyObserver(&observer);
  EXPECT_EQ(kMaximumDecisionLogs, observer.messages().size());
}

TEST_F(PreviewsLoggerTest,
       PreviewsLoggerOnlyKeepsCertainNumberOfNavigationLogs) {
  PreviewsType type = PreviewsType::OFFLINE;
  const GURL url("http://www.url_.com/url_");
  const base::Time time = base::Time::Now();

  for (size_t i = 0; i < 2 * kMaximumNavigationLogs; ++i) {
    logger_->LogPreviewNavigation(url, type, true /* opt_out */, time);
  }

  TestPreviewsLoggerObserver observer;
  logger_->AddAndNotifyObserver(&observer);
  EXPECT_EQ(kMaximumNavigationLogs, observer.messages().size());
}

TEST_F(PreviewsLoggerTest, ObserverIsNotifiedOfHistoricalNavigationsWhenAdded) {
  // Non historical log event.
  logger_->LogMessage("Event_", "Some description_",
                      GURL("http://www.url_.com/url_"), base::Time::Now());

  PreviewsEligibilityReason reason =
      PreviewsEligibilityReason::BLACKLIST_UNAVAILABLE;
  PreviewsType type = PreviewsType::LOFI;
  const GURL urls[] = {
      GURL("http://www.url_0.com/url_0"), GURL("http://www.url_1.com/url_1"),
      GURL("http://www.url_2.com/url_2"),
  };
  const base::Time times[] = {
      base::Time::FromJsTime(-413696806000),  // Nov 21 1956 20:13:14 UTC
      base::Time::FromJsTime(758620800000),   // Jan 15 1994 08:00:00 UTC
      base::Time::FromJsTime(1581696550000),  // Feb 14 2020 16:09:10 UTC
  };

  // Logging decisions and navigations events in mixed orders.
  logger_->LogPreviewDecisionMade(reason, urls[0], times[0], type);
  logger_->LogPreviewNavigation(urls[1], type, true /* opt_out */, times[1]);
  logger_->LogPreviewDecisionMade(reason, urls[2], times[2], type);

  TestPreviewsLoggerObserver observer;
  logger_->AddAndNotifyObserver(&observer);

  // Check correct ordering of the historical log messages.
  auto received_messages = observer.messages();

  const size_t expected_size = 3;
  EXPECT_EQ(expected_size, received_messages.size());

  const std::string expected_types[] = {
      kPreviewsDecisionMadeEventType, kPreviewsNavigationEventType,
      kPreviewsDecisionMadeEventType,
  };

  for (size_t i = 0; i < expected_size; i++) {
    EXPECT_EQ(expected_types[i], received_messages[i].event_type);
    EXPECT_EQ(urls[i], received_messages[i].url);
    EXPECT_EQ(times[i], received_messages[i].time);
  }
}

TEST_F(PreviewsLoggerTest, ObserversOnNewMessageIsCalledWithCorrectParams) {
  TestPreviewsLoggerObserver observers[3];

  const size_t number_of_obs = 3;
  for (size_t i = 0; i < number_of_obs; i++) {
    logger_->AddAndNotifyObserver(&observers[i]);
  }

  std::string type = "Event_";
  std::string description = "Some description";
  GURL url("http://www.url_.com/url_");
  base::Time now = base::Time::Now();
  logger_->LogMessage(type, description, url, now);

  const size_t expected_size = 1;
  for (size_t i = 0; i < number_of_obs; i++) {
    EXPECT_EQ(expected_size, observers[i].messages().size());
    EXPECT_EQ(type, observers[i].message()->event_type);
    EXPECT_EQ(description, observers[i].message()->event_description);
    EXPECT_EQ(url, observers[i].message()->url);
    EXPECT_EQ(now, observers[i].message()->time);
  }
}

TEST_F(PreviewsLoggerTest, RemovedObserverIsNotNotified) {
  TestPreviewsLoggerObserver observers[3];

  const size_t number_of_obs = 3;
  for (size_t i = 0; i < number_of_obs; i++) {
    logger_->AddAndNotifyObserver(&observers[i]);
  }

  const size_t removed_observer = 1;
  logger_->RemoveObserver(&observers[removed_observer]);

  std::string type = "Event_";
  std::string description = "Some description";
  GURL url("http://www.url_.com/url_");
  base::Time now = base::Time::Now();
  logger_->LogMessage(type, description, url, now);

  const size_t expected_size = 0;
  EXPECT_EQ(expected_size, observers[removed_observer].messages().size());

  for (size_t i = 0; i < number_of_obs; i++) {
    if (i != removed_observer) {
      EXPECT_EQ(type, observers[i].message()->event_type);
      EXPECT_EQ(description, observers[i].message()->event_description);
      EXPECT_EQ(url, observers[i].message()->url);
      EXPECT_EQ(now, observers[i].message()->time);
    }
  }
}

}  // namespace

}  // namespace previews
