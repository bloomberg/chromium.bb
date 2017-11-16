// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/core/previews_logger.h"

#include <string>
#include <unordered_map>
#include <vector>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_split.h"
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
  TestPreviewsLoggerObserver()
      : host_blacklisted_called_(false),
        user_status_change_calls_(0),
        blacklist_cleared_called_(false),
        blacklist_ignored_(false),
        last_removed_notified_(false) {}

  ~TestPreviewsLoggerObserver() override {}

  // PreviewsLoggerObserver:
  void OnNewMessageLogAdded(
      const PreviewsLogger::MessageLog& message) override {
    message_ = base::MakeUnique<PreviewsLogger::MessageLog>(message);
    messages_.push_back(*message_);
  }
  void OnNewBlacklistedHost(const std::string& host, base::Time time) override {
    host_blacklisted_called_ = true;
    blacklisted_hosts_[host] = time;
  }
  void OnUserBlacklistedStatusChange(bool blacklisted) override {
    ++user_status_change_calls_;
    user_blacklisted_ = blacklisted;
  }
  void OnBlacklistCleared(base::Time time) override {
    blacklist_cleared_called_ = true;
    blacklist_cleared_time_ = time;
  }
  void OnIgnoreBlacklistDecisionStatusChanged(bool ignored) override {
    blacklist_ignored_ = ignored;
  }
  void OnLastObserverRemove() override { last_removed_notified_ = true; }

  // Expose the passed in MessageLog for testing.
  PreviewsLogger::MessageLog* message() const { return message_.get(); }

  // Expose the received MessageLogs for testing.
  const std::vector<PreviewsLogger::MessageLog>& messages() const {
    return messages_;
  };

  // Expose blacklist events info for testing.
  const std::unordered_map<std::string, base::Time>& blacklisted_hosts() {
    return blacklisted_hosts_;
  }
  bool host_blacklisted_called() const { return host_blacklisted_called_; }
  size_t user_status_change_calls() const { return user_status_change_calls_; }
  bool blacklist_cleared_called() const { return blacklist_cleared_called_; }
  bool user_blacklisted() const { return user_blacklisted_; }
  base::Time blacklist_cleared_time() const { return blacklist_cleared_time_; }

  // Expose whether PreviewsBlackList decisions are ignored or not.
  bool blacklist_ignored() const { return blacklist_ignored_; }

  // Expose whether observer is notified that it is the last observer to be
  // removed for testing.
  bool last_removed_notified() { return last_removed_notified_; }

 private:
  // Received messages.
  std::vector<PreviewsLogger::MessageLog> messages_;

  // The passed in MessageLog in OnNewMessageLogAdded.
  std::unique_ptr<PreviewsLogger::MessageLog> message_;

  // Received blacklisted event info.
  std::unordered_map<std::string, base::Time> blacklisted_hosts_;
  bool user_blacklisted_;
  base::Time blacklist_cleared_time_;
  bool host_blacklisted_called_;
  size_t user_status_change_calls_;
  bool blacklist_cleared_called_;

  // BlacklistPreviews decision is ignored or not.
  bool blacklist_ignored_;

  // Whether |this| is the last observer to be removed.
  bool last_removed_notified_;
};

class PreviewsLoggerTest : public testing::Test {
 public:
  PreviewsLoggerTest() {}

  ~PreviewsLoggerTest() override {}

  void SetUp() override { logger_ = base::MakeUnique<PreviewsLogger>(); }

  std::string LogPreviewDecisionAndGetReasonDescription(
      PreviewsEligibilityReason reason) {
    const base::Time time = base::Time::Now();
    PreviewsType type = PreviewsType::OFFLINE;
    const GURL url("http://www.url_a.com/url");
    TestPreviewsLoggerObserver observer;
    logger_->AddAndNotifyObserver(&observer);
    logger_->LogPreviewDecisionMade(reason, url, time, type);

    auto actual = observer.messages();

    const size_t expected_size = 1;
    EXPECT_EQ(expected_size, actual.size());

    std::vector<std::string> description_parts = base::SplitStringUsingSubstr(
        actual[0].event_description, "preview - ", base::TRIM_WHITESPACE,
        base::SPLIT_WANT_NONEMPTY);
    return description_parts[1];
  }

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
      "Offline preview - Blacklist failed to be created";
  EXPECT_EQ(kPreviewsDecisionMadeEventType, actual[0].event_type);
  EXPECT_EQ(expected_description_a, actual[0].event_description);
  EXPECT_EQ(url_a, actual[0].url);
  EXPECT_EQ(time, actual[0].time);

  std::string expected_description_b = "LoFi preview - Network not slow";
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

  std::string expected_description_a = "Offline preview - user opt-out: True";
  EXPECT_EQ(kPreviewsNavigationEventType, actual[0].event_type);
  EXPECT_EQ(expected_description_a, actual[0].event_description);
  EXPECT_EQ(url_a, actual[0].url);
  EXPECT_EQ(time, actual[0].time);

  std::string expected_description_b = "LoFi preview - user opt-out: False";
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

TEST_F(PreviewsLoggerTest, LogPreviewDecisionDescriptionAllowed) {
  std::string actual_description = LogPreviewDecisionAndGetReasonDescription(
      PreviewsEligibilityReason::ALLOWED);
  std::string expected_description = "Allowed";
  EXPECT_EQ(expected_description, actual_description);
}

TEST_F(PreviewsLoggerTest, LogPreviewDecisionDescriptionUnavailabe) {
  std::string actual_description = LogPreviewDecisionAndGetReasonDescription(
      PreviewsEligibilityReason::BLACKLIST_UNAVAILABLE);
  std::string expected_description = "Blacklist failed to be created";
  EXPECT_EQ(expected_description, actual_description);
}

TEST_F(PreviewsLoggerTest, LogPreviewDecisionDescriptionNotLoaded) {
  std::string actual_description = LogPreviewDecisionAndGetReasonDescription(
      PreviewsEligibilityReason::BLACKLIST_DATA_NOT_LOADED);
  std::string expected_description = "Blacklist not loaded from disk yet";
  EXPECT_EQ(expected_description, actual_description);
}

TEST_F(PreviewsLoggerTest, LogPreviewDecisionDescriptionRecentlyOptedOut) {
  std::string actual_description = LogPreviewDecisionAndGetReasonDescription(
      PreviewsEligibilityReason::USER_RECENTLY_OPTED_OUT);
  std::string expected_description = "User recently opted out";
  EXPECT_EQ(expected_description, actual_description);
}

TEST_F(PreviewsLoggerTest, LogPreviewDecisionDescriptionBlacklisted) {
  std::string actual_description = LogPreviewDecisionAndGetReasonDescription(
      PreviewsEligibilityReason::USER_BLACKLISTED);
  std::string expected_description = "All previews are blacklisted";
  EXPECT_EQ(expected_description, actual_description);
}

TEST_F(PreviewsLoggerTest, LogPreviewDecisionDescriptionHostBlacklisted) {
  std::string actual_description = LogPreviewDecisionAndGetReasonDescription(
      PreviewsEligibilityReason::HOST_BLACKLISTED);
  std::string expected_description =
      "All previews on this host are blacklisted";
  EXPECT_EQ(expected_description, actual_description);
}

TEST_F(PreviewsLoggerTest, LogPreviewDecisionDescriptionNetworkUnavailable) {
  std::string actual_description = LogPreviewDecisionAndGetReasonDescription(
      PreviewsEligibilityReason::NETWORK_QUALITY_UNAVAILABLE);
  std::string expected_description = "Network quality unavailable";
  EXPECT_EQ(expected_description, actual_description);
}

TEST_F(PreviewsLoggerTest, LogPreviewDecisionDescriptionNetworkNotSlow) {
  std::string actual_description = LogPreviewDecisionAndGetReasonDescription(
      PreviewsEligibilityReason::NETWORK_NOT_SLOW);
  std::string expected_description = "Network not slow";

  EXPECT_EQ(expected_description, actual_description);
}

TEST_F(PreviewsLoggerTest, LogPreviewDecisionDescriptionReloadDisallowed) {
  std::string actual_description = LogPreviewDecisionAndGetReasonDescription(
      PreviewsEligibilityReason::RELOAD_DISALLOWED);
  std::string expected_description =
      "Page reloads do not show previews for this preview type";
  EXPECT_EQ(expected_description, actual_description);
}

TEST_F(PreviewsLoggerTest, LogPreviewDecisionDescriptionServerRules) {
  std::string actual_description = LogPreviewDecisionAndGetReasonDescription(
      PreviewsEligibilityReason::HOST_BLACKLISTED_BY_SERVER);
  std::string expected_description = "Host blacklisted by server rules";
  EXPECT_EQ(expected_description, actual_description);
}

TEST_F(PreviewsLoggerTest, LogPreviewDecisionDescriptionNotWhitelisedByServer) {
  std::string actual_description = LogPreviewDecisionAndGetReasonDescription(
      PreviewsEligibilityReason::HOST_NOT_WHITELISTED_BY_SERVER);
  std::string expected_description = "Host not whitelisted by server rules";
  EXPECT_EQ(expected_description, actual_description);
}

TEST_F(PreviewsLoggerTest,
       LogPreviewDecisionDescriptionAllowedWithoutServerOptimizationHints) {
  std::string actual_description = LogPreviewDecisionAndGetReasonDescription(
      PreviewsEligibilityReason::ALLOWED_WITHOUT_OPTIMIZATION_HINTS);
  std::string expected_description = "Allowed (but without server rule check)";
  EXPECT_EQ(expected_description, actual_description);
}

TEST_F(PreviewsLoggerTest, NotifyObserversOfNewBlacklistedHost) {
  TestPreviewsLoggerObserver observers[3];

  const size_t number_of_obs = 3;
  for (size_t i = 0; i < number_of_obs; i++) {
    logger_->AddAndNotifyObserver(&observers[i]);
  }

  const size_t removed_observer = 1;
  logger_->RemoveObserver(&observers[removed_observer]);

  const std::string expected_host = "example.com";
  const base::Time expected_time = base::Time::Now();
  const size_t expected_size = 1;
  logger_->OnNewBlacklistedHost(expected_host, expected_time);

  for (size_t i = 0; i < number_of_obs; i++) {
    if (i != removed_observer) {
      EXPECT_TRUE(observers[i].host_blacklisted_called());
      EXPECT_EQ(expected_size, observers[i].blacklisted_hosts().size());
      EXPECT_EQ(expected_time,
                observers[i].blacklisted_hosts().find(expected_host)->second);
    }
  }
  EXPECT_FALSE(observers[removed_observer].host_blacklisted_called());
}

TEST_F(PreviewsLoggerTest, NotifyObserversWhenUserBlacklisted) {
  TestPreviewsLoggerObserver observers[3];

  const size_t number_of_obs = 3;
  for (size_t i = 0; i < number_of_obs; i++) {
    logger_->AddAndNotifyObserver(&observers[i]);
  }

  const size_t removed_observer = 1;
  logger_->RemoveObserver(&observers[removed_observer]);
  logger_->OnUserBlacklistedStatusChange(true /* blacklisted */);
  const size_t expected_times = 2;

  for (size_t i = 0; i < number_of_obs; i++) {
    if (i != removed_observer) {
      EXPECT_EQ(expected_times, observers[i].user_status_change_calls());
      EXPECT_TRUE(observers[i].user_blacklisted());
    }
  }
  EXPECT_EQ(expected_times - 1,
            observers[removed_observer].user_status_change_calls());
}

TEST_F(PreviewsLoggerTest, NotifyObserversWhenUserNotBlacklisted) {
  TestPreviewsLoggerObserver observers[3];

  const size_t number_of_obs = 3;
  for (size_t i = 0; i < number_of_obs; i++) {
    logger_->AddAndNotifyObserver(&observers[i]);
  }

  const size_t removed_observer = 1;
  logger_->RemoveObserver(&observers[removed_observer]);
  logger_->OnUserBlacklistedStatusChange(false /* blacklisted */);
  const size_t expected_times = 2;

  for (size_t i = 0; i < number_of_obs; i++) {
    if (i != removed_observer) {
      EXPECT_EQ(expected_times, observers[i].user_status_change_calls());
      EXPECT_FALSE(observers[i].user_blacklisted());
    }
  }
  EXPECT_EQ(expected_times - 1,
            observers[removed_observer].user_status_change_calls());
}

TEST_F(PreviewsLoggerTest, NotifyObserversWhenBlacklistCleared) {
  TestPreviewsLoggerObserver observers[3];

  const size_t number_of_obs = 3;
  for (size_t i = 0; i < number_of_obs; i++) {
    logger_->AddAndNotifyObserver(&observers[i]);
  }

  const size_t removed_observer = 1;
  logger_->RemoveObserver(&observers[removed_observer]);

  const base::Time expected_time = base::Time::Now();
  logger_->OnBlacklistCleared(expected_time);

  for (size_t i = 0; i < number_of_obs; i++) {
    if (i != removed_observer) {
      EXPECT_TRUE(observers[i].blacklist_cleared_called());
      EXPECT_EQ(expected_time, observers[i].blacklist_cleared_time());
    }
  }
  EXPECT_FALSE(observers[removed_observer].blacklist_cleared_called());
}

TEST_F(PreviewsLoggerTest, ObserverNotifiedOfUserBlacklistedStateWhenAdded) {
  TestPreviewsLoggerObserver observer;

  const std::string host0 = "example0.com";
  const std::string host1 = "example1.com";
  const base::Time time0 = base::Time::Now();
  const base::Time time1 = base::Time::Now();

  logger_->OnUserBlacklistedStatusChange(true /* blacklisted */);
  logger_->OnNewBlacklistedHost(host0, time0);
  logger_->OnNewBlacklistedHost(host1, time1);
  logger_->AddAndNotifyObserver(&observer);

  const size_t expected_times = 1;
  EXPECT_EQ(expected_times, observer.user_status_change_calls());
  const size_t expected_size = 2;
  EXPECT_EQ(expected_size, observer.blacklisted_hosts().size());
  EXPECT_EQ(time0, observer.blacklisted_hosts().find(host0)->second);
  EXPECT_EQ(time1, observer.blacklisted_hosts().find(host1)->second);
}

TEST_F(PreviewsLoggerTest, NotifyObserversBlacklistIgnoredUpdate) {
  TestPreviewsLoggerObserver observers[3];
  const size_t number_of_obs = 3;
  for (size_t i = 0; i < number_of_obs; i++) {
    logger_->AddAndNotifyObserver(&observers[i]);
  }

  for (size_t i = 0; i < number_of_obs; i++) {
    EXPECT_FALSE(observers[i].blacklist_ignored());
  }

  const size_t removed_observer = 1;
  logger_->RemoveObserver(&observers[removed_observer]);
  logger_->OnIgnoreBlacklistDecisionStatusChanged(true /* ignored */);
  for (size_t i = 0; i < number_of_obs; i++) {
    if (i != removed_observer) {
      EXPECT_TRUE(observers[i].blacklist_ignored());
    }
  }
  EXPECT_FALSE(observers[removed_observer].blacklist_ignored());
}

TEST_F(PreviewsLoggerTest, ObserverNotifiedOfBlacklistIgnoreStatusOnAdd) {
  logger_->OnIgnoreBlacklistDecisionStatusChanged(true /* ignored */);
  TestPreviewsLoggerObserver observer;
  EXPECT_FALSE(observer.blacklist_ignored());
  logger_->AddAndNotifyObserver(&observer);
  EXPECT_TRUE(observer.blacklist_ignored());
}

TEST_F(PreviewsLoggerTest, LastObserverRemovedIsNotified) {
  TestPreviewsLoggerObserver observers[3];
  const size_t number_of_obs = 3;
  for (size_t i = 0; i < number_of_obs; i++) {
    logger_->AddAndNotifyObserver(&observers[i]);
  }
  for (size_t i = 0; i < number_of_obs; i++) {
    logger_->RemoveObserver(&observers[i]);
  }
  EXPECT_TRUE(observers[number_of_obs - 1].last_removed_notified());
}

}  // namespace

}  // namespace previews
