// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/ml/user_activity_logger.h"

#include <memory>
#include <vector>

#include "base/timer/timer.h"
#include "chrome/browser/chromeos/power/ml/idle_event_notifier.h"
#include "chrome/browser/chromeos/power/ml/user_activity_event.pb.h"
#include "chrome/browser/chromeos/power/ml/user_activity_logger_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace power {
namespace ml {

void EqualEvent(const UserActivityEvent::Event& expected_event,
                const UserActivityEvent::Event& result_event) {
  EXPECT_EQ(expected_event.type(), result_event.type());
  EXPECT_EQ(expected_event.reason(), result_event.reason());
}

// Testing logger delegate.
class TestingUserActivityLoggerDelegate : public UserActivityLoggerDelegate {
 public:
  TestingUserActivityLoggerDelegate() = default;
  ~TestingUserActivityLoggerDelegate() override = default;

  const std::vector<UserActivityEvent>& events() const { return events_; }

  void LogActivity(const UserActivityEvent& event) override {
    events_.push_back(event);
  }

 private:
  std::vector<UserActivityEvent> events_;

  DISALLOW_COPY_AND_ASSIGN(TestingUserActivityLoggerDelegate);
};

class UserActivityLoggerTest : public testing::Test {
 public:
  UserActivityLoggerTest() : activity_logger_(&delegate_) {}

  ~UserActivityLoggerTest() override = default;

 protected:
  void ReportUserActivity(const ui::Event* event) {
    activity_logger_.OnUserActivity(event);
  }

  void ReportIdleEvent(const IdleEventNotifier::ActivityData& data) {
    activity_logger_.OnIdleEventObserved(data);
  }

  const std::vector<UserActivityEvent>& GetEvents() {
    return delegate_.events();
  }

 private:
  TestingUserActivityLoggerDelegate delegate_;
  UserActivityLogger activity_logger_;
};

// After an idle event, we have a ui::Event, we should expect one
// UserActivityEvent.
TEST_F(UserActivityLoggerTest, LogAfterIdleEvent) {
  // Trigger an idle event.
  ReportIdleEvent({base::Time::Now()});
  ReportUserActivity(nullptr);

  const auto& events = GetEvents();
  ASSERT_EQ(1U, events.size());

  UserActivityEvent::Event expected_event;
  expected_event.set_type(UserActivityEvent::Event::REACTIVATE);
  expected_event.set_reason(UserActivityEvent::Event::USER_ACTIVITY);
  EqualEvent(expected_event, events[0].event());
}

// Get a user event before an idle event, we should not log it.
TEST_F(UserActivityLoggerTest, LogBeforeIdleEvent) {
  ReportUserActivity(nullptr);
  // Trigger an idle event.
  ReportIdleEvent({base::Time::Now()});

  EXPECT_EQ(0U, GetEvents().size());
}

// Get a user event, then an idle event, then another user event,
// we should log the last one.
TEST_F(UserActivityLoggerTest, LogSecondEvent) {
  ReportUserActivity(nullptr);
  // Trigger an idle event.
  ReportIdleEvent({base::Time::Now()});
  // Another user event.
  ReportUserActivity(nullptr);

  const auto& events = GetEvents();
  ASSERT_EQ(1U, events.size());

  UserActivityEvent::Event expected_event;
  expected_event.set_type(UserActivityEvent::Event::REACTIVATE);
  expected_event.set_reason(UserActivityEvent::Event::USER_ACTIVITY);
  EqualEvent(expected_event, events[0].event());
}

// Log multiple events.
TEST_F(UserActivityLoggerTest, LogMultipleEvents) {
  // Trigger an idle event.
  ReportIdleEvent({base::Time::Now()});
  // First user event.
  ReportUserActivity(nullptr);

  // Trigger an idle event.
  ReportIdleEvent({base::Time::Now()});
  // Second user event.
  ReportUserActivity(nullptr);

  const auto& events = GetEvents();
  ASSERT_EQ(2U, events.size());

  UserActivityEvent::Event expected_event;
  expected_event.set_type(UserActivityEvent::Event::REACTIVATE);
  expected_event.set_reason(UserActivityEvent::Event::USER_ACTIVITY);
  EqualEvent(expected_event, events[0].event());
  EqualEvent(expected_event, events[1].event());
}

}  // namespace ml
}  // namespace power
}  // namespace chromeos
