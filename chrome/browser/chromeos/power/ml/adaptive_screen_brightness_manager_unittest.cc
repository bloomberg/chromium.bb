// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/ml/adaptive_screen_brightness_manager.h"

#include <memory>
#include <vector>

#include "base/test/test_mock_time_task_runner.h"
#include "base/time/clock.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/accessibility/magnification_manager.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/power/ml/adaptive_screen_brightness_ukm_logger.h"
#include "chrome/browser/chromeos/power/ml/fake_boot_clock.h"
#include "chrome/browser/chromeos/power/ml/screen_brightness_event.pb.h"
#include "chromeos/dbus/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "chromeos/dbus/power_manager_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/events/base_event_utils.cc"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/gfx/geometry/point.h"

namespace chromeos {
namespace power {
namespace ml {

const int kInactivityDurationSecs =
    AdaptiveScreenBrightnessManager::kInactivityDuration.InSeconds();

// Testing ukm logger.
class TestingAdaptiveScreenBrightnessUkmLogger
    : public AdaptiveScreenBrightnessUkmLogger {
 public:
  TestingAdaptiveScreenBrightnessUkmLogger() = default;
  ~TestingAdaptiveScreenBrightnessUkmLogger() override = default;

  const std::vector<ScreenBrightnessEvent>& screen_brightness_events() const {
    return screen_brightness_events_;
  }

  // AdaptiveScreenBrightnessUkmLogger overrides:
  void LogActivity(
      const ScreenBrightnessEvent& screen_brightness_event) override {
    screen_brightness_events_.push_back(screen_brightness_event);
  }

 private:
  std::vector<ScreenBrightnessEvent> screen_brightness_events_;

  DISALLOW_COPY_AND_ASSIGN(TestingAdaptiveScreenBrightnessUkmLogger);
};

class AdaptiveScreenBrightnessManagerTest : public testing::Test {
 public:
  AdaptiveScreenBrightnessManagerTest()
      : task_runner_(base::MakeRefCounted<base::TestMockTimeTaskRunner>(
            base::TestMockTimeTaskRunner::Type::kBoundToThread)),
        scoped_context_(task_runner_.get()) {
    fake_power_manager_client_.Init(nullptr);
    viz::mojom::VideoDetectorObserverPtr observer;
    std::unique_ptr<base::RepeatingTimer> periodic_timer =
        std::make_unique<base::RepeatingTimer>();
    periodic_timer->SetTaskRunner(task_runner_);
    screen_brightness_manager_ =
        std::make_unique<AdaptiveScreenBrightnessManager>(
            &ukm_logger_, &user_activity_detector_, &fake_power_manager_client_,
            nullptr, nullptr, mojo::MakeRequest(&observer),
            std::move(periodic_timer), task_runner_->GetMockClock(),
            std::make_unique<FakeBootClock>(task_runner_,
                                            base::TimeDelta::FromSeconds(10)));
  }

  ~AdaptiveScreenBrightnessManagerTest() override = default;

 protected:
  void ReportUserActivity(const ui::Event* const event) {
    screen_brightness_manager_->OnUserActivity(event);
  }

  void ReportPowerChangeEvent(
      const power_manager::PowerSupplyProperties::ExternalPower power,
      const float battery_percent) {
    power_manager::PowerSupplyProperties proto;
    proto.set_external_power(power);
    proto.set_battery_percent(battery_percent);
    fake_power_manager_client_.UpdatePowerProperties(proto);
  }

  void ReportLidEvent(const chromeos::PowerManagerClient::LidState state) {
    fake_power_manager_client_.SetLidState(state, base::TimeTicks::UnixEpoch());
  }

  void ReportTabletModeEvent(
      const chromeos::PowerManagerClient::TabletMode mode) {
    fake_power_manager_client_.SetTabletMode(mode,
                                             base::TimeTicks::UnixEpoch());
  }

  void ReportBrightnessChangeEvent(
      const double level,
      const power_manager::BacklightBrightnessChange_Cause cause) {
    power_manager::BacklightBrightnessChange change;
    change.set_percent(level);
    change.set_cause(cause);
    screen_brightness_manager_->ScreenBrightnessChanged(change);
  }

  void ReportVideoStart() {
    screen_brightness_manager_->OnVideoActivityStarted();
  }

  void ReportVideoEnd() { screen_brightness_manager_->OnVideoActivityEnded(); }

  void FireTimer() { screen_brightness_manager_->OnTimerFired(); }

  void InitializeBrightness(const double level) {
    screen_brightness_manager_->OnReceiveScreenBrightnessPercent(level);
  }

  void FastForwardTimeBySecs(const int seconds) {
    task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(seconds));
  }

  TestingAdaptiveScreenBrightnessUkmLogger ukm_logger_;
  const gfx::Point kEventLocation = gfx::Point(90, 90);
  const ui::MouseEvent kMouseEvent = ui::MouseEvent(ui::ET_MOUSE_MOVED,
                                                    kEventLocation,
                                                    kEventLocation,
                                                    base::TimeTicks(),
                                                    0,
                                                    0);

 private:
  const scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  const base::TestMockTimeTaskRunner::ScopedContext scoped_context_;
  chromeos::FakeChromeUserManager fake_user_manager_;

  ui::UserActivityDetector user_activity_detector_;
  chromeos::FakePowerManagerClient fake_power_manager_client_;
  std::unique_ptr<AdaptiveScreenBrightnessManager> screen_brightness_manager_;

  DISALLOW_COPY_AND_ASSIGN(AdaptiveScreenBrightnessManagerTest);
};

TEST_F(AdaptiveScreenBrightnessManagerTest, PeriodicLogging) {
  InitializeBrightness(75.0f);
  ReportPowerChangeEvent(power_manager::PowerSupplyProperties::AC, 23.0f);
  ReportVideoStart();
  ReportLidEvent(chromeos::PowerManagerClient::LidState::OPEN);
  ReportTabletModeEvent(chromeos::PowerManagerClient::TabletMode::UNSUPPORTED);

  FireTimer();

  const std::vector<ScreenBrightnessEvent>& screen_brightness_events =
      ukm_logger_.screen_brightness_events();
  ASSERT_EQ(1U, screen_brightness_events.size());

  const ScreenBrightnessEvent::Features& features =
      screen_brightness_events[0].features();
  EXPECT_FLOAT_EQ(23.0, features.env_data().battery_percent());
  EXPECT_FALSE(features.env_data().on_battery());
  EXPECT_TRUE(features.activity_data().is_video_playing());
  EXPECT_EQ(ScreenBrightnessEvent::Features::EnvData::LAPTOP,
            features.env_data().device_mode());

  const ScreenBrightnessEvent::Event& event =
      screen_brightness_events[0].event();
  EXPECT_EQ(75, event.brightness());
}

TEST_F(AdaptiveScreenBrightnessManagerTest, BrightnessChange) {
  ReportBrightnessChangeEvent(
      30.0f, power_manager::
                 BacklightBrightnessChange_Cause_EXTERNAL_POWER_DISCONNECTED);
  ReportBrightnessChangeEvent(
      40.0f, power_manager::BacklightBrightnessChange_Cause_USER_REQUEST);
  ReportBrightnessChangeEvent(
      20.0f, power_manager::BacklightBrightnessChange_Cause_USER_REQUEST);

  const std::vector<ScreenBrightnessEvent>& screen_brightness_events =
      ukm_logger_.screen_brightness_events();
  ASSERT_EQ(3U, screen_brightness_events.size());
  const ScreenBrightnessEvent::Event& event =
      screen_brightness_events[0].event();
  EXPECT_EQ(30, event.brightness());
  EXPECT_EQ(ScreenBrightnessEvent::Event::EXTERNAL_POWER_DISCONNECTED,
            event.reason());
  const ScreenBrightnessEvent::Event& event1 =
      screen_brightness_events[1].event();
  EXPECT_EQ(40, event1.brightness());
  EXPECT_EQ(ScreenBrightnessEvent::Event::USER_UP, event1.reason());
  const ScreenBrightnessEvent::Event& event2 =
      screen_brightness_events[2].event();
  EXPECT_EQ(20, event2.brightness());
  EXPECT_EQ(ScreenBrightnessEvent::Event::USER_DOWN, event2.reason());
}

TEST_F(AdaptiveScreenBrightnessManagerTest, NoUserEvents) {
  InitializeBrightness(75.0f);

  FastForwardTimeBySecs(2);
  FireTimer();

  const std::vector<ScreenBrightnessEvent>& screen_brightness_events =
      ukm_logger_.screen_brightness_events();
  // This counts logging events, not user events.
  ASSERT_EQ(1U, screen_brightness_events.size());
  const ScreenBrightnessEvent::Features& features =
      screen_brightness_events[0].features();
  EXPECT_EQ(0, features.activity_data().last_activity_time_sec());
  EXPECT_EQ(0, features.activity_data().recent_time_active_sec());
}

TEST_F(AdaptiveScreenBrightnessManagerTest, NullUserActivity) {
  InitializeBrightness(75.0f);

  FastForwardTimeBySecs(1);
  ReportUserActivity(nullptr);
  FastForwardTimeBySecs(2);
  FireTimer();

  const std::vector<ScreenBrightnessEvent>& screen_brightness_events =
      ukm_logger_.screen_brightness_events();
  ASSERT_EQ(1U, screen_brightness_events.size());
  const ScreenBrightnessEvent::Features& features =
      screen_brightness_events[0].features();
  EXPECT_EQ(0, features.activity_data().last_activity_time_sec());
  EXPECT_EQ(0, features.activity_data().recent_time_active_sec());
}

TEST_F(AdaptiveScreenBrightnessManagerTest, OneUserEvent) {
  InitializeBrightness(75.0f);

  FastForwardTimeBySecs(1);
  ReportUserActivity(&kMouseEvent);
  FastForwardTimeBySecs(2);
  FireTimer();

  const std::vector<ScreenBrightnessEvent>& screen_brightness_events =
      ukm_logger_.screen_brightness_events();
  ASSERT_EQ(1U, screen_brightness_events.size());
  const ScreenBrightnessEvent::Features& features =
      screen_brightness_events[0].features();
  EXPECT_EQ(2, features.activity_data().last_activity_time_sec());
  EXPECT_EQ(0, features.activity_data().recent_time_active_sec());
}

TEST_F(AdaptiveScreenBrightnessManagerTest, TwoUserEventsSameActivity) {
  InitializeBrightness(75.0f);

  FastForwardTimeBySecs(1);
  ReportUserActivity(&kMouseEvent);

  FastForwardTimeBySecs(5);
  ReportUserActivity(&kMouseEvent);

  FastForwardTimeBySecs(2);
  FireTimer();

  const std::vector<ScreenBrightnessEvent>& screen_brightness_events =
      ukm_logger_.screen_brightness_events();
  ASSERT_EQ(1U, screen_brightness_events.size());
  const ScreenBrightnessEvent::Features& features =
      screen_brightness_events[0].features();
  EXPECT_EQ(2, features.activity_data().last_activity_time_sec());
  EXPECT_EQ(5, features.activity_data().recent_time_active_sec());
}

TEST_F(AdaptiveScreenBrightnessManagerTest, TwoUserEventsDifferentActivities) {
  InitializeBrightness(75.0f);

  FastForwardTimeBySecs(1);
  ReportUserActivity(&kMouseEvent);

  FastForwardTimeBySecs(kInactivityDurationSecs + 5);
  ReportUserActivity(&kMouseEvent);

  FastForwardTimeBySecs(2);
  FireTimer();

  const std::vector<ScreenBrightnessEvent>& screen_brightness_events =
      ukm_logger_.screen_brightness_events();
  ASSERT_EQ(1U, screen_brightness_events.size());
  const ScreenBrightnessEvent::Features& features =
      screen_brightness_events[0].features();
  EXPECT_EQ(2, features.activity_data().last_activity_time_sec());
  EXPECT_EQ(0, features.activity_data().recent_time_active_sec());
}

TEST_F(AdaptiveScreenBrightnessManagerTest,
       MultipleUserEventsMultipleActivities) {
  InitializeBrightness(75.0f);

  FastForwardTimeBySecs(1);
  ReportUserActivity(&kMouseEvent);

  FastForwardTimeBySecs(5);
  ReportUserActivity(&kMouseEvent);

  FastForwardTimeBySecs(kInactivityDurationSecs + 5);
  ReportUserActivity(&kMouseEvent);

  FastForwardTimeBySecs(kInactivityDurationSecs + 10);
  ReportUserActivity(&kMouseEvent);

  FastForwardTimeBySecs(2);
  ReportUserActivity(&kMouseEvent);

  FastForwardTimeBySecs(2);
  ReportUserActivity(&kMouseEvent);

  FastForwardTimeBySecs(2);
  FireTimer();

  const std::vector<ScreenBrightnessEvent>& screen_brightness_events =
      ukm_logger_.screen_brightness_events();
  ASSERT_EQ(1U, screen_brightness_events.size());
  const ScreenBrightnessEvent::Features& features =
      screen_brightness_events[0].features();
  EXPECT_EQ(2, features.activity_data().last_activity_time_sec());
  EXPECT_EQ(4, features.activity_data().recent_time_active_sec());
}

TEST_F(AdaptiveScreenBrightnessManagerTest, VideoStartStop) {
  InitializeBrightness(75.0f);

  FastForwardTimeBySecs(2);
  ReportVideoStart();

  FastForwardTimeBySecs(5);
  FireTimer();

  FastForwardTimeBySecs(kInactivityDurationSecs + 40);
  FireTimer();

  const std::vector<ScreenBrightnessEvent>& screen_brightness_events =
      ukm_logger_.screen_brightness_events();
  ASSERT_EQ(2U, screen_brightness_events.size());
  const ScreenBrightnessEvent::Features& features =
      screen_brightness_events[0].features();
  EXPECT_EQ(0, features.activity_data().last_activity_time_sec());
  EXPECT_EQ(5, features.activity_data().recent_time_active_sec());
  const ScreenBrightnessEvent::Features& features1 =
      screen_brightness_events[1].features();
  EXPECT_EQ(0, features1.activity_data().last_activity_time_sec());
  EXPECT_EQ(kInactivityDurationSecs + 45,
            features1.activity_data().recent_time_active_sec());

  FastForwardTimeBySecs(10);
  ReportVideoEnd();

  FastForwardTimeBySecs(5);
  FireTimer();

  FastForwardTimeBySecs(kInactivityDurationSecs + 45);
  FireTimer();

  ASSERT_EQ(4U, screen_brightness_events.size());
  const ScreenBrightnessEvent::Features& features2 =
      screen_brightness_events[2].features();
  EXPECT_EQ(5, features2.activity_data().last_activity_time_sec());
  EXPECT_EQ(kInactivityDurationSecs + 55,
            features2.activity_data().recent_time_active_sec());
  const ScreenBrightnessEvent::Features& features3 =
      screen_brightness_events[3].features();
  EXPECT_EQ(kInactivityDurationSecs + 50,
            features3.activity_data().last_activity_time_sec());
  EXPECT_EQ(kInactivityDurationSecs + 55,
            features3.activity_data().recent_time_active_sec());
}

TEST_F(AdaptiveScreenBrightnessManagerTest, VideoStartStopWithUserEvents) {
  InitializeBrightness(75.0f);

  FastForwardTimeBySecs(1);
  ReportUserActivity(&kMouseEvent);

  FastForwardTimeBySecs(2);
  ReportVideoStart();

  FastForwardTimeBySecs(5);
  FireTimer();

  FastForwardTimeBySecs(kInactivityDurationSecs + 40);
  FireTimer();

  FastForwardTimeBySecs(4);
  ReportUserActivity(&kMouseEvent);

  FastForwardTimeBySecs(6);
  FireTimer();

  const std::vector<ScreenBrightnessEvent>& screen_brightness_events =
      ukm_logger_.screen_brightness_events();
  ASSERT_EQ(3U, screen_brightness_events.size());
  const ScreenBrightnessEvent::Features& features =
      screen_brightness_events[0].features();
  EXPECT_EQ(0, features.activity_data().last_activity_time_sec());
  EXPECT_EQ(7, features.activity_data().recent_time_active_sec());
  const ScreenBrightnessEvent::Features& features1 =
      screen_brightness_events[1].features();
  EXPECT_EQ(0, features1.activity_data().last_activity_time_sec());
  EXPECT_EQ(kInactivityDurationSecs + 47,
            features1.activity_data().recent_time_active_sec());
  const ScreenBrightnessEvent::Features& features2 =
      screen_brightness_events[2].features();
  EXPECT_EQ(0, features2.activity_data().last_activity_time_sec());
  EXPECT_EQ(kInactivityDurationSecs + 57,
            features2.activity_data().recent_time_active_sec());

  FastForwardTimeBySecs(10);
  ReportVideoEnd();

  FastForwardTimeBySecs(5);
  FireTimer();

  FastForwardTimeBySecs(kInactivityDurationSecs + 45);
  FireTimer();

  ASSERT_EQ(5U, screen_brightness_events.size());
  const ScreenBrightnessEvent::Features& features3 =
      screen_brightness_events[3].features();
  EXPECT_EQ(5, features3.activity_data().last_activity_time_sec());
  EXPECT_EQ(kInactivityDurationSecs + 67,
            features3.activity_data().recent_time_active_sec());
  const ScreenBrightnessEvent::Features& features4 =
      screen_brightness_events[4].features();
  EXPECT_EQ(kInactivityDurationSecs + 50,
            features4.activity_data().last_activity_time_sec());
  EXPECT_EQ(kInactivityDurationSecs + 67,
            features4.activity_data().recent_time_active_sec());
}

TEST_F(AdaptiveScreenBrightnessManagerTest, UserEventCounts) {
  InitializeBrightness(75.0f);

  FastForwardTimeBySecs(1);
  ReportUserActivity(&kMouseEvent);

  const ui::TouchEvent kTouchEvent(
      ui::ET_TOUCH_PRESSED, kEventLocation, base::TimeTicks(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));
  ReportUserActivity(&kTouchEvent);
  ReportUserActivity(&kTouchEvent);

  const ui::KeyEvent kKeyEvent(
      ui::ET_KEY_PRESSED, ui::VKEY_A, ui::DomCode::US_A, 0,
      ui::DomKey::FromCharacter('a'), base::TimeTicks());
  ReportUserActivity(&kKeyEvent);
  ReportUserActivity(&kKeyEvent);
  ReportUserActivity(&kKeyEvent);

  const ui::PointerEvent kStylusEvent(
      ui::ET_POINTER_MOVED, kEventLocation, gfx::Point(), ui::EF_NONE, 0,
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_PEN, 0),
      base::TimeTicks());
  ReportUserActivity(&kStylusEvent);
  ReportUserActivity(&kStylusEvent);
  ReportUserActivity(&kStylusEvent);
  ReportUserActivity(&kStylusEvent);

  FastForwardTimeBySecs(2);
  FireTimer();

  const std::vector<ScreenBrightnessEvent>& screen_brightness_events =
      ukm_logger_.screen_brightness_events();
  ASSERT_EQ(1U, screen_brightness_events.size());
  const ScreenBrightnessEvent::Features& features =
      screen_brightness_events[0].features();
  EXPECT_EQ(1, features.activity_data().num_recent_mouse_events());
  EXPECT_EQ(2, features.activity_data().num_recent_touch_events());
  EXPECT_EQ(3, features.activity_data().num_recent_key_events());
  EXPECT_EQ(4, features.activity_data().num_recent_stylus_events());
}

}  // namespace ml
}  // namespace power
}  // namespace chromeos
