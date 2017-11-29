// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/ml/user_activity_logger.h"

#include <memory>
#include <vector>

#include "base/single_thread_task_runner.h"
#include "base/test/scoped_task_environment.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/power/ml/idle_event_notifier.h"
#include "chrome/browser/chromeos/power/ml/user_activity_event.pb.h"
#include "chrome/browser/chromeos/power/ml/user_activity_logger_delegate.h"
#include "chromeos/dbus/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "chromeos/dbus/power_manager_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/user_activity/user_activity_detector.h"

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

  // UserActivityLoggerDelegate overrides:
  void LogActivity(const UserActivityEvent& event) override {
    events_.push_back(event);
  }

 private:
  std::vector<UserActivityEvent> events_;

  DISALLOW_COPY_AND_ASSIGN(TestingUserActivityLoggerDelegate);
};

class UserActivityLoggerTest : public testing::Test {
 public:
  UserActivityLoggerTest() : task_runner_(base::ThreadTaskRunnerHandle::Get()) {
    viz::mojom::VideoDetectorObserverPtr observer;
    idle_event_notifier_ = std::make_unique<IdleEventNotifier>(
        &fake_power_manager_client_, mojo::MakeRequest(&observer));
    activity_logger_ = std::make_unique<UserActivityLogger>(
        &delegate_, idle_event_notifier_.get(), &user_activity_detector_,
        &fake_power_manager_client_);
  }

  ~UserActivityLoggerTest() override = default;

 protected:
  void ReportUserActivity(const ui::Event* event) {
    activity_logger_->OnUserActivity(event);
  }

  void ReportIdleEvent(const IdleEventNotifier::ActivityData& data) {
    activity_logger_->OnIdleEventObserved(data);
  }

  void ReportLidEvent(chromeos::PowerManagerClient::LidState state) {
    fake_power_manager_client_.SetLidState(state, base::TimeTicks::Now());
  }

  void ReportPowerChangeEvent(
      power_manager::PowerSupplyProperties::ExternalPower power,
      float battery_percent) {
    power_manager::PowerSupplyProperties proto;
    proto.set_external_power(power);
    proto.set_battery_percent(battery_percent);
    fake_power_manager_client_.UpdatePowerProperties(proto);
  }

  void ReportTabletModeEvent(chromeos::PowerManagerClient::TabletMode mode) {
    fake_power_manager_client_.SetTabletMode(mode, base::TimeTicks::Now());
  }

  const std::vector<UserActivityEvent>& GetEvents() {
    return delegate_.events();
  }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  ui::UserActivityDetector user_activity_detector_;
  TestingUserActivityLoggerDelegate delegate_;
  std::unique_ptr<IdleEventNotifier> idle_event_notifier_;
  chromeos::FakePowerManagerClient fake_power_manager_client_;
  std::unique_ptr<UserActivityLogger> activity_logger_;
};

// After an idle event, we have a ui::Event, we should expect one
// UserActivityEvent.
TEST_F(UserActivityLoggerTest, LogAfterIdleEvent) {
  // Trigger an idle event.
  base::Time now = base::Time::UnixEpoch();
  ReportIdleEvent({now, now});
  ReportUserActivity(nullptr);

  const std::vector<UserActivityEvent>& events = GetEvents();
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
  base::Time now = base::Time::UnixEpoch();
  ReportIdleEvent({now, now});

  EXPECT_EQ(0U, GetEvents().size());
}

// Get a user event, then an idle event, then another user event,
// we should log the last one.
TEST_F(UserActivityLoggerTest, LogSecondEvent) {
  ReportUserActivity(nullptr);
  // Trigger an idle event.
  base::Time now = base::Time::UnixEpoch();
  ReportIdleEvent({now, now});
  // Another user event.
  ReportUserActivity(nullptr);

  const std::vector<UserActivityEvent>& events = GetEvents();
  ASSERT_EQ(1U, events.size());

  UserActivityEvent::Event expected_event;
  expected_event.set_type(UserActivityEvent::Event::REACTIVATE);
  expected_event.set_reason(UserActivityEvent::Event::USER_ACTIVITY);
  EqualEvent(expected_event, events[0].event());
}

// Log multiple events.
TEST_F(UserActivityLoggerTest, LogMultipleEvents) {
  // Trigger an idle event.
  base::Time now = base::Time::UnixEpoch();
  ReportIdleEvent({now, now});
  // First user event.
  ReportUserActivity(nullptr);

  // Trigger an idle event.
  now = base::Time::UnixEpoch();
  ReportIdleEvent({now, now});
  // Second user event.
  ReportUserActivity(nullptr);

  const std::vector<UserActivityEvent>& events = GetEvents();
  ASSERT_EQ(2U, events.size());

  UserActivityEvent::Event expected_event;
  expected_event.set_type(UserActivityEvent::Event::REACTIVATE);
  expected_event.set_reason(UserActivityEvent::Event::USER_ACTIVITY);
  EqualEvent(expected_event, events[0].event());
  EqualEvent(expected_event, events[1].event());
}

// Test feature extraction.
TEST_F(UserActivityLoggerTest, FeatureExtraction) {
  ReportLidEvent(chromeos::PowerManagerClient::LidState::OPEN);
  ReportTabletModeEvent(chromeos::PowerManagerClient::TabletMode::UNSUPPORTED);
  ReportPowerChangeEvent(power_manager::PowerSupplyProperties::AC, 23.0f);

  ReportIdleEvent({});
  ReportUserActivity(nullptr);

  const auto& events = GetEvents();
  ASSERT_EQ(1U, events.size());

  const UserActivityEvent::Features& features = events[0].features();
  EXPECT_EQ(UserActivityEvent::Features::CLAMSHELL, features.device_mode());
  EXPECT_EQ(23.0f, features.battery_percent());
  EXPECT_DOUBLE_EQ(false, features.on_battery());
}

}  // namespace ml
}  // namespace power
}  // namespace chromeos
