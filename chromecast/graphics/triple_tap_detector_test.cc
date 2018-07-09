// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/graphics/triple_tap_detector.h"

#include "base/run_loop.h"
#include "base/test/simple_test_tick_clock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/window.h"
#include "ui/events/event_utils.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/test/events_test_utils.h"
#include "ui/wm/core/default_screen_position_client.h"

using testing::_;
using testing::Eq;

namespace chromecast {

namespace {
constexpr gfx::Point kTestTapLocation(50, 50);
constexpr int kTapLengthMs = 50;
}  // namespace

class MockTripleTapDetectorDelegate : public TripleTapDetectorDelegate {
 public:
  ~MockTripleTapDetectorDelegate() override = default;
  MOCK_METHOD1(OnTripleTap, void(const gfx::Point&));
};

class TripleTapDetectorTest : public aura::test::AuraTestBase {
 public:
  ~TripleTapDetectorTest() override = default;

  void SetUp() override {
    aura::test::AuraTestBase::SetUp();

    screen_position_client_.reset(new wm::DefaultScreenPositionClient());
    aura::client::SetScreenPositionClient(root_window(),
                                          screen_position_client_.get());

    triple_tap_delegate_ = std::make_unique<MockTripleTapDetectorDelegate>();
    triple_tap_detector_ = std::make_unique<TripleTapDetector>(
        root_window(), triple_tap_delegate_.get());

    generator_.reset(new ui::test::EventGenerator(root_window()));

    // Tests fail if time is ever 0.
    simulated_clock_.Advance(base::TimeDelta::FromMilliseconds(10));
    // ui takes ownership of the tick clock.
    ui::SetEventTickClockForTesting(&simulated_clock_);
  }

  void TearDown() override {
    triple_tap_detector_.reset();
    aura::test::AuraTestBase::TearDown();
  }

  // Pause the minimum amount of tap to trigger a double tap.
  void DoubleTapPause() {
    simulated_clock_.Advance(gesture_detector_config_.double_tap_min_time);
  }

  // Pause just past the maximum amount of time to trigger a double tap.
  void TooLongPause() {
    simulated_clock_.Advance(gesture_detector_config_.double_tap_timeout);
    simulated_clock_.Advance(base::TimeDelta::FromMilliseconds(1));
    triple_tap_detector_->triple_tap_timer_.Stop();
    triple_tap_detector_->OnTripleTapIntervalTimerFired();
  }

  // Simulate a tap event.
  void Tap(const gfx::Point& tap_point) {
    ui::TouchEvent press(
        ui::ET_TOUCH_PRESSED, tap_point, simulated_clock_.NowTicks(),
        ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH,
                           ui::PointerDetails::kUnknownPointerId));
    generator_->Dispatch(&press);
    simulated_clock_.Advance(base::TimeDelta::FromMilliseconds(kTapLengthMs));
    ui::TouchEvent release(
        ui::ET_TOUCH_RELEASED, tap_point, simulated_clock_.NowTicks(),
        ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH,
                           ui::PointerDetails::kUnknownPointerId));
    generator_->Dispatch(&release);
  }

  void Tap() { Tap(kTestTapLocation); }

  TripleTapDetector& detector() { return *triple_tap_detector_; }
  MockTripleTapDetectorDelegate& delegate() { return *triple_tap_delegate_; }

 private:
  ui::GestureDetector::Config gesture_detector_config_;

  std::unique_ptr<aura::client::ScreenPositionClient> screen_position_client_;

  std::unique_ptr<TripleTapDetector> triple_tap_detector_;
  std::unique_ptr<MockTripleTapDetectorDelegate> triple_tap_delegate_;
  base::SimpleTestTickClock simulated_clock_;
  std::unique_ptr<ui::test::EventGenerator> generator_;
};

// Verify that a simple correct triple tap triggers the delegate.
TEST_F(TripleTapDetectorTest, TripleTap) {
  EXPECT_CALL(delegate(), OnTripleTap(Eq(kTestTapLocation)));

  detector().set_enabled(true);

  Tap();
  DoubleTapPause();
  Tap();
  DoubleTapPause();
  Tap();

  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

// Verify that no triple taps are detected when the detector is not enabled.
TEST_F(TripleTapDetectorTest, InactiveNoTriple) {
  EXPECT_CALL(delegate(), OnTripleTap(Eq(kTestTapLocation))).Times(0);

  Tap();
  DoubleTapPause();
  Tap();
  DoubleTapPause();
  Tap();

  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

// Verify it's not a triple tap if the pause from the first tap to the second
// tap is too long.
TEST_F(TripleTapDetectorTest, FirstTapTooLong) {
  EXPECT_CALL(delegate(), OnTripleTap(Eq(kTestTapLocation))).Times(0);

  detector().set_enabled(true);

  Tap();
  TooLongPause();
  Tap();
  DoubleTapPause();
  Tap();

  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

// Verify it's not a triple tap if the pause from the second tap to the last tap
// is too long.
TEST_F(TripleTapDetectorTest, LastTapTooLong) {
  EXPECT_CALL(delegate(), OnTripleTap(Eq(kTestTapLocation))).Times(0);

  detector().set_enabled(true);

  Tap();
  DoubleTapPause();
  Tap();
  TooLongPause();
  Tap();

  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

}  // namespace chromecast
