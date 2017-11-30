// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/ml/user_activity_logger.h"

#include <memory>
#include <vector>

#include "base/test/test_mock_time_task_runner.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/power/ml/idle_event_notifier.h"
#include "chrome/browser/chromeos/power/ml/user_activity_event.pb.h"
#include "chrome/browser/chromeos/power/ml/user_activity_logger_delegate.h"
#include "chromeos/dbus/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/idle.pb.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "chromeos/dbus/power_manager_client.h"
#include "components/session_manager/session_manager_types.h"
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
  void UpdateOpenTabsURLs() override {}

 private:
  std::vector<UserActivityEvent> events_;

  DISALLOW_COPY_AND_ASSIGN(TestingUserActivityLoggerDelegate);
};

class UserActivityLoggerTest : public testing::Test {
 public:
  UserActivityLoggerTest()
      : task_runner_(base::MakeRefCounted<base::TestMockTimeTaskRunner>(
            base::TestMockTimeTaskRunner::Type::kBoundToThread)),
        scoped_context_(task_runner_.get()) {
    fake_power_manager_client_.Init(nullptr);
    viz::mojom::VideoDetectorObserverPtr observer;
    idle_event_notifier_ = std::make_unique<IdleEventNotifier>(
        &fake_power_manager_client_, &user_activity_detector_,
        mojo::MakeRequest(&observer));
    activity_logger_ = std::make_unique<UserActivityLogger>(
        &delegate_, idle_event_notifier_.get(), &user_activity_detector_,
        &fake_power_manager_client_, &session_manager_,
        mojo::MakeRequest(&observer));
    activity_logger_->SetTaskRunnerForTesting(task_runner_);
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
    fake_power_manager_client_.SetLidState(state, base::TimeTicks::UnixEpoch());
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
    fake_power_manager_client_.SetTabletMode(mode,
                                             base::TimeTicks::UnixEpoch());
  }

  void ReportVideoStart() { activity_logger_->OnVideoActivityStarted(); }

  void ReportScreenIdle() {
    power_manager::ScreenIdleState proto;
    proto.set_off(true);
    fake_power_manager_client_.SendScreenIdleStateChanged(proto);
  }

  void ReportScreenLocked() {
    session_manager_.SetSessionState(session_manager::SessionState::LOCKED);
  }

  void ReportIdleSleep() { fake_power_manager_client_.SendSuspendDone(); }

  const std::vector<UserActivityEvent>& GetEvents() {
    return delegate_.events();
  }

  const scoped_refptr<base::TestMockTimeTaskRunner>& GetTaskRunner() {
    return task_runner_;
  }

 private:
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  const base::TestMockTimeTaskRunner::ScopedContext scoped_context_;

  ui::UserActivityDetector user_activity_detector_;
  TestingUserActivityLoggerDelegate delegate_;
  std::unique_ptr<IdleEventNotifier> idle_event_notifier_;
  chromeos::FakePowerManagerClient fake_power_manager_client_;
  session_manager::SessionManager session_manager_;
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

TEST_F(UserActivityLoggerTest, UserCloseLid) {
  ReportLidEvent(chromeos::PowerManagerClient::LidState::OPEN);
  // Trigger an idle event.
  base::Time now = base::Time::UnixEpoch();
  ReportIdleEvent({now, now});

  ReportLidEvent(chromeos::PowerManagerClient::LidState::CLOSED);
  const auto& events = GetEvents();
  ASSERT_EQ(1U, events.size());

  UserActivityEvent::Event expected_event;
  expected_event.set_type(UserActivityEvent::Event::OFF);
  expected_event.set_reason(UserActivityEvent::Event::LID_CLOSED);
  EqualEvent(expected_event, events[0].event());
}

TEST_F(UserActivityLoggerTest, PowerChangeActivity) {
  ReportPowerChangeEvent(power_manager::PowerSupplyProperties::AC, 23.0f);
  // Trigger an idle event.
  base::Time now = base::Time::UnixEpoch();
  ReportIdleEvent({now, now});

  // We don't care about battery percentage change, but only power source.
  ReportPowerChangeEvent(power_manager::PowerSupplyProperties::AC, 25.0f);
  ReportPowerChangeEvent(power_manager::PowerSupplyProperties::DISCONNECTED,
                         28.0f);
  const auto& events = GetEvents();
  ASSERT_EQ(1U, events.size());

  UserActivityEvent::Event expected_event;
  expected_event.set_type(UserActivityEvent::Event::REACTIVATE);
  expected_event.set_reason(UserActivityEvent::Event::POWER_CHANGED);
  EqualEvent(expected_event, events[0].event());
}

TEST_F(UserActivityLoggerTest, VideoActivity) {
  // Trigger an idle event.
  base::Time now = base::Time::UnixEpoch();
  ReportIdleEvent({now, now});

  ReportVideoStart();
  const auto& events = GetEvents();
  ASSERT_EQ(1U, events.size());

  UserActivityEvent::Event expected_event;
  expected_event.set_type(UserActivityEvent::Event::REACTIVATE);
  expected_event.set_reason(UserActivityEvent::Event::VIDEO_ACTIVITY);
  EqualEvent(expected_event, events[0].event());
}

TEST_F(UserActivityLoggerTest, SystemIdle) {
  // Trigger an idle event.
  base::Time now = base::Time::UnixEpoch();
  ReportIdleEvent({now, now});

  ReportScreenIdle();
  GetTaskRunner()->FastForwardUntilNoTasksRemain();

  const auto& events = GetEvents();
  ASSERT_EQ(1U, events.size());

  UserActivityEvent::Event expected_event;
  expected_event.set_type(UserActivityEvent::Event::TIMEOUT);
  expected_event.set_reason(UserActivityEvent::Event::SCREEN_OFF);
  EqualEvent(expected_event, events[0].event());
}

// Test system idle interrupt by user activity.
// We should only observe user activity.
TEST_F(UserActivityLoggerTest, SystemIdleInterrupted) {
  // Trigger an idle event.
  base::Time now = base::Time::UnixEpoch();
  ReportIdleEvent({now, now});

  ReportScreenIdle();
  // User interruptted after 1 second.
  GetTaskRunner()->FastForwardBy(base::TimeDelta::FromSeconds(1));
  ReportUserActivity(nullptr);
  GetTaskRunner()->FastForwardUntilNoTasksRemain();

  const auto& events = GetEvents();
  ASSERT_EQ(1U, events.size());

  UserActivityEvent::Event expected_event;
  expected_event.set_type(UserActivityEvent::Event::REACTIVATE);
  expected_event.set_reason(UserActivityEvent::Event::USER_ACTIVITY);
  EqualEvent(expected_event, events[0].event());
}

TEST_F(UserActivityLoggerTest, ScreenLock) {
  // Trigger an idle event.
  base::Time now = base::Time::UnixEpoch();
  ReportIdleEvent({now, now});

  ReportScreenLocked();
  const auto& events = GetEvents();
  ASSERT_EQ(1U, events.size());

  UserActivityEvent::Event expected_event;
  expected_event.set_type(UserActivityEvent::Event::OFF);
  expected_event.set_reason(UserActivityEvent::Event::SCREEN_LOCK);
  EqualEvent(expected_event, events[0].event());
}

TEST_F(UserActivityLoggerTest, IdleSleep) {
  // Trigger an idle event.
  base::Time now = base::Time::UnixEpoch();
  ReportIdleEvent({now, now});

  ReportIdleSleep();
  const auto& events = GetEvents();
  ASSERT_EQ(1U, events.size());

  UserActivityEvent::Event expected_event;
  expected_event.set_type(UserActivityEvent::Event::TIMEOUT);
  expected_event.set_reason(UserActivityEvent::Event::IDLE_SLEEP);
  EqualEvent(expected_event, events[0].event());
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
