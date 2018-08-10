// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/graphics/gestures/side_swipe_detector.h"

#include <memory>

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_mock_time_task_runner.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/event_generator_delegate_aura.h"
#include "ui/aura/window.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/wm/core/default_screen_position_client.h"

// Gmock matchers and actions that we use below.
using testing::_;
using testing::AnyOf;
using testing::Eq;
using testing::Return;

namespace chromecast {
namespace test {

namespace {

constexpr base::TimeDelta kTimeDelay = base::TimeDelta::FromMilliseconds(100);
constexpr int kSwipeDistance = 50;
constexpr int kNumSteps = 5;
constexpr gfx::Point kZeroPoint{0, 0};
constexpr base::TimeDelta kHoldCornerDelay =
    base::TimeDelta::FromMilliseconds(3500);
constexpr gfx::Point kNWCorner{5, 5};

}  // namespace

class TestEventGeneratorDelegate
    : public aura::test::EventGeneratorDelegateAura {
 public:
  explicit TestEventGeneratorDelegate(aura::Window* root_window)
      : root_window_(root_window) {}
  ~TestEventGeneratorDelegate() override = default;

  // EventGeneratorDelegateAura overrides:
  aura::WindowTreeHost* GetHostAt(const gfx::Point& point) const override {
    return root_window_->GetHost();
  }

  aura::client::ScreenPositionClient* GetScreenPositionClient(
      const aura::Window* window) const override {
    return aura::client::GetScreenPositionClient(root_window_);
  }

 private:
  aura::Window* root_window_;

  DISALLOW_COPY_AND_ASSIGN(TestEventGeneratorDelegate);
};

// TODO(rdaum): Make this use gmock for all events instead of just for corners.
class TestSideSwipeGestureHandler : public CastGestureHandler {
 public:
  ~TestSideSwipeGestureHandler() override = default;

  bool CanHandleSwipe(CastSideSwipeOrigin swipe_origin) override {
    return handle_swipe_;
  }

  void HandleSideSwipeBegin(CastSideSwipeOrigin swipe_origin,
                            const gfx::Point& touch_location) override {
    if (handle_swipe_) {
      begin_swipe_origin_ = swipe_origin;
      begin_swipe_point_ = touch_location;
    }
  }

  void HandleSideSwipeEnd(CastSideSwipeOrigin swipe_origin,
                          const gfx::Point& gesture_event) override {
    end_swipe_origin_ = swipe_origin;
    end_swipe_point_ = gesture_event;
  }

  void SetHandleSwipe(bool handle_swipe) { handle_swipe_ = handle_swipe; }

  CastSideSwipeOrigin begin_swipe_origin() const { return begin_swipe_origin_; }
  gfx::Point begin_swipe_point() const { return begin_swipe_point_; }

  CastSideSwipeOrigin end_swipe_origin() const { return end_swipe_origin_; }
  gfx::Point end_swipe_point() const { return end_swipe_point_; }

  // Mocks.
  MOCK_METHOD2(HandleScreenExit,
               void(CastSideSwipeOrigin side,
                    const gfx::Point& touch_location));
  MOCK_METHOD2(HandleScreenEnter,
               void(CastSideSwipeOrigin side,
                    const gfx::Point& touch_location));
  MOCK_CONST_METHOD0(HandledCornerHolds, CastGestureHandler::Corner());
  MOCK_METHOD2(HandleCornerHold,
               void(CastGestureHandler::Corner corner_origin,
                    const ui::TouchEvent& touch_event));
  MOCK_METHOD2(HandleCornerHoldEnd,
               void(CastGestureHandler::Corner corner_origin,
                    const ui::TouchEvent& touch_event));

 private:
  bool handle_swipe_ = true;

  CastSideSwipeOrigin begin_swipe_origin_ = CastSideSwipeOrigin::NONE;
  gfx::Point begin_swipe_point_;

  CastSideSwipeOrigin end_swipe_origin_ = CastSideSwipeOrigin::NONE;
  gfx::Point end_swipe_point_;
};

// Event sink to check for events that get through (or don't get through) after
// the system gesture handler handles them.
class TestEventHandler : public ui::EventHandler {
 public:
  TestEventHandler() : EventHandler(), num_touch_events_received_(0) {}

  void OnTouchEvent(ui::TouchEvent* event) override {
    num_touch_events_received_++;
  }

  int NumTouchEventsReceived() const { return num_touch_events_received_; }

 private:
  int num_touch_events_received_;
};

class SideSwipeDetectorTest : public aura::test::AuraTestBase {
 public:
  ~SideSwipeDetectorTest() override = default;

  void SetUp() override {
    aura::test::AuraTestBase::SetUp();

    screen_position_client_.reset(new wm::DefaultScreenPositionClient());
    aura::client::SetScreenPositionClient(root_window(),
                                          screen_position_client_.get());

    gesture_handler_ = std::make_unique<TestSideSwipeGestureHandler>();
    side_swipe_detector_ = std::make_unique<SideSwipeDetector>(
        gesture_handler_.get(), root_window());
    test_event_handler_ = std::make_unique<TestEventHandler>();
    root_window()->AddPostTargetHandler(test_event_handler_.get());

    mock_task_runner_ = base::MakeRefCounted<base::TestMockTimeTaskRunner>(
        base::Time::Now(), base::TimeTicks::Now());
    auto mock_timer = std::make_unique<base::OneShotTimer>(
        mock_task_runner_->GetMockTickClock());
    mock_timer->SetTaskRunner(mock_task_runner_);
    ui::SetEventTickClockForTesting(mock_task_runner_->GetMockTickClock());
    side_swipe_detector_->SetTimerForTesting(std::move(mock_timer));

    EXPECT_CALL(test_gesture_handler(), HandledCornerHolds())
        .WillRepeatedly(Return(CastGestureHandler::NO_CORNERS));
  }

  void TearDown() override {
    side_swipe_detector_.reset();
    gesture_handler_.reset();

    aura::test::AuraTestBase::TearDown();
  }

  // Simulate a tap-hold event in one place.
  void Hold(const gfx::Point& tap_point, const base::TimeDelta& delta) {
    ui::TouchEvent press(
        ui::ET_TOUCH_PRESSED, tap_point, mock_clock()->NowTicks(),
        ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH,
                           ui::PointerDetails::kUnknownPointerId));
    GetEventGenerator().Dispatch(&press);
    mock_task_runner()->AdvanceMockTickClock(delta);
    mock_task_runner()->FastForwardBy(delta);
    ui::TouchEvent release(
        ui::ET_TOUCH_RELEASED, tap_point, mock_clock()->NowTicks(),
        ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH,
                           ui::PointerDetails::kUnknownPointerId));
    GetEventGenerator().Dispatch(&release);
  }

  void Drag(const gfx::Point& start_point,
            const base::TimeDelta& start_hold_time,
            const base::TimeDelta& drag_time,
            const gfx::Point& end_point,
            bool end_release = true) {
    ui::TouchEvent press(
        ui::ET_TOUCH_PRESSED, start_point, mock_clock()->NowTicks(),
        ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH,
                           ui::PointerDetails::kUnknownPointerId));
    GetEventGenerator().Dispatch(&press);
    mock_task_runner()->AdvanceMockTickClock(start_hold_time);
    mock_task_runner()->FastForwardBy(start_hold_time);

    ui::TouchEvent move(
        ui::ET_TOUCH_MOVED, end_point, mock_clock()->NowTicks(),
        ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH,
                           ui::PointerDetails::kUnknownPointerId));
    GetEventGenerator().Dispatch(&move);
    mock_task_runner()->AdvanceMockTickClock(drag_time);
    mock_task_runner()->FastForwardBy(drag_time);

    if (end_release) {
      ui::TouchEvent release(
          ui::ET_TOUCH_RELEASED, end_point, mock_clock()->NowTicks(),
          ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH,
                             ui::PointerDetails::kUnknownPointerId));
      GetEventGenerator().Dispatch(&release);
    }
  }

  ui::test::EventGenerator& GetEventGenerator() {
    if (!event_generator_) {
      event_generator_.reset(new ui::test::EventGenerator(
          new TestEventGeneratorDelegate(root_window())));
    }
    return *event_generator_.get();
  }

  TestSideSwipeGestureHandler& test_gesture_handler() {
    return *gesture_handler_;
  }

  TestEventHandler& test_event_handler() { return *test_event_handler_; }

  base::TestMockTimeTaskRunner* mock_task_runner() const {
    return mock_task_runner_.get();
  }

  const base::TickClock* mock_clock() const {
    return mock_task_runner_->GetMockTickClock();
  }

 private:
  std::unique_ptr<aura::client::ScreenPositionClient> screen_position_client_;
  std::unique_ptr<ui::test::EventGenerator> event_generator_;

  std::unique_ptr<SideSwipeDetector> side_swipe_detector_;
  std::unique_ptr<TestEventHandler> test_event_handler_;
  std::unique_ptr<TestSideSwipeGestureHandler> gesture_handler_;
  scoped_refptr<base::TestMockTimeTaskRunner> mock_task_runner_;
};

// Test that initialization works and initial state is clean.
TEST_F(SideSwipeDetectorTest, Initialization) {
  EXPECT_EQ(CastSideSwipeOrigin::NONE,
            test_gesture_handler().begin_swipe_origin());
  EXPECT_EQ(kZeroPoint, test_gesture_handler().begin_swipe_point());
  EXPECT_EQ(CastSideSwipeOrigin::NONE,
            test_gesture_handler().end_swipe_origin());
  EXPECT_EQ(kZeroPoint, test_gesture_handler().end_swipe_point());
  EXPECT_EQ(0, test_event_handler().NumTouchEventsReceived());
}

// A swipe in the middle of the screen should produce no system gesture.
TEST_F(SideSwipeDetectorTest, SwipeWithNoSystemGesture) {
  gfx::Point drag_point(root_window()->bounds().width() / 2,
                        root_window()->bounds().height() / 2);
  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.GestureScrollSequence(drag_point,
                                  drag_point - gfx::Vector2d(0, kSwipeDistance),
                                  kTimeDelay, kNumSteps);

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(CastSideSwipeOrigin::NONE,
            test_gesture_handler().begin_swipe_origin());
  EXPECT_EQ(kZeroPoint, test_gesture_handler().begin_swipe_point());
  EXPECT_EQ(CastSideSwipeOrigin::NONE,
            test_gesture_handler().end_swipe_origin());
  EXPECT_EQ(kZeroPoint, test_gesture_handler().end_swipe_point());
  EXPECT_NE(0, test_event_handler().NumTouchEventsReceived());
}

TEST_F(SideSwipeDetectorTest, SwipeFromLeft) {
  gfx::Point drag_point(0, root_window()->bounds().height() / 2);
  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.GestureScrollSequence(drag_point,
                                  drag_point + gfx::Vector2d(kSwipeDistance, 0),
                                  kTimeDelay, kNumSteps);

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(CastSideSwipeOrigin::LEFT,
            test_gesture_handler().begin_swipe_origin());
  EXPECT_NE(kZeroPoint, test_gesture_handler().begin_swipe_point());
  EXPECT_EQ(CastSideSwipeOrigin::LEFT,
            test_gesture_handler().end_swipe_origin());
  EXPECT_NE(kZeroPoint, test_gesture_handler().end_swipe_point());
  EXPECT_EQ(0, test_event_handler().NumTouchEventsReceived());
}

TEST_F(SideSwipeDetectorTest, SwipeFromRight) {
  gfx::Point drag_point(root_window()->bounds().width(),
                        root_window()->bounds().height() / 2);
  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.GestureScrollSequence(drag_point,
                                  drag_point - gfx::Vector2d(kSwipeDistance, 0),
                                  kTimeDelay, kNumSteps);

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(CastSideSwipeOrigin::RIGHT,
            test_gesture_handler().begin_swipe_origin());
  EXPECT_NE(kZeroPoint, test_gesture_handler().begin_swipe_point());
  EXPECT_EQ(CastSideSwipeOrigin::RIGHT,
            test_gesture_handler().end_swipe_origin());
  EXPECT_NE(kZeroPoint, test_gesture_handler().end_swipe_point());
  EXPECT_EQ(0, test_event_handler().NumTouchEventsReceived());
}

TEST_F(SideSwipeDetectorTest, SwipeFromTop) {
  gfx::Point drag_point(root_window()->bounds().width() / 2, 0);
  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.GestureScrollSequence(drag_point,
                                  drag_point + gfx::Vector2d(0, kSwipeDistance),
                                  kTimeDelay, kNumSteps);

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(CastSideSwipeOrigin::TOP,
            test_gesture_handler().begin_swipe_origin());
  EXPECT_NE(kZeroPoint, test_gesture_handler().begin_swipe_point());
  EXPECT_EQ(CastSideSwipeOrigin::TOP,
            test_gesture_handler().end_swipe_origin());
  EXPECT_NE(kZeroPoint, test_gesture_handler().end_swipe_point());
  EXPECT_EQ(0, test_event_handler().NumTouchEventsReceived());
}

TEST_F(SideSwipeDetectorTest, SwipeFromBottom) {
  gfx::Point drag_point(root_window()->bounds().width() / 2,
                        root_window()->bounds().height());
  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.GestureScrollSequence(drag_point,
                                  drag_point - gfx::Vector2d(0, kSwipeDistance),
                                  kTimeDelay, kNumSteps);

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(CastSideSwipeOrigin::BOTTOM,
            test_gesture_handler().begin_swipe_origin());
  EXPECT_NE(kZeroPoint, test_gesture_handler().begin_swipe_point());
  EXPECT_EQ(CastSideSwipeOrigin::BOTTOM,
            test_gesture_handler().end_swipe_origin());
  EXPECT_NE(kZeroPoint, test_gesture_handler().end_swipe_point());
  EXPECT_EQ(0, test_event_handler().NumTouchEventsReceived());
}

// Test that ignoring the gesture at its beginning will make it so the swipe
// is not produced at the end.
TEST_F(SideSwipeDetectorTest, SwipeUnhandledIgnored) {
  test_gesture_handler().SetHandleSwipe(false);

  gfx::Point drag_point(root_window()->bounds().width() / 2,
                        root_window()->bounds().height());
  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.GestureScrollSequence(drag_point,
                                  drag_point - gfx::Vector2d(0, kSwipeDistance),
                                  kTimeDelay, kNumSteps);

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(CastSideSwipeOrigin::NONE,
            test_gesture_handler().begin_swipe_origin());
  EXPECT_EQ(kZeroPoint, test_gesture_handler().begin_swipe_point());
  EXPECT_EQ(CastSideSwipeOrigin::NONE,
            test_gesture_handler().end_swipe_origin());
  EXPECT_EQ(kZeroPoint, test_gesture_handler().end_swipe_point());
  EXPECT_NE(0, test_event_handler().NumTouchEventsReceived());
}

TEST_F(SideSwipeDetectorTest, ScreenEnter) {
  EXPECT_CALL(test_gesture_handler(),
              HandleScreenEnter(Eq(CastSideSwipeOrigin::TOP), _))
      .Times(1);
  EXPECT_CALL(test_gesture_handler(),
              HandleScreenExit(Eq(CastSideSwipeOrigin::TOP), _))
      .Times(0);

  test_gesture_handler().SetHandleSwipe(false);

  gfx::Point drag_point(root_window()->bounds().width() / 2, 0);
  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.GestureScrollSequence(drag_point,
                                  drag_point + gfx::Vector2d(0, kSwipeDistance),
                                  kTimeDelay, kNumSteps);

  base::RunLoop().RunUntilIdle();
}

TEST_F(SideSwipeDetectorTest, ScreenExit) {
  EXPECT_CALL(test_gesture_handler(),
              HandleScreenExit(Eq(CastSideSwipeOrigin::TOP), _))
      .Times(1);
  EXPECT_CALL(test_gesture_handler(),
              HandleScreenEnter(Eq(CastSideSwipeOrigin::TOP), _))
      .Times(0);

  test_gesture_handler().SetHandleSwipe(false);

  gfx::Point drag_point(root_window()->bounds().width() / 2, 0);
  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.GestureScrollSequence(drag_point + gfx::Vector2d(0, kSwipeDistance),
                                  drag_point, kTimeDelay, kNumSteps);

  base::RunLoop().RunUntilIdle();
}

// Test corner hold.
TEST_F(SideSwipeDetectorTest, CornerHoldTrigger) {
  EXPECT_CALL(test_gesture_handler(), HandledCornerHolds())
      .WillRepeatedly(Return(CastGestureHandler::TOP_LEFT_CORNER));
  EXPECT_CALL(test_gesture_handler(),
              HandleCornerHold(Eq(CastGestureHandler::TOP_LEFT_CORNER), _))
      .Times(1);
  EXPECT_CALL(test_gesture_handler(),
              HandleCornerHoldEnd(Eq(CastGestureHandler::TOP_LEFT_CORNER), _))
      .Times(1);
  Hold(kNWCorner, kHoldCornerDelay);

  base::RunLoop().RunUntilIdle();
}

// Test that it's a corner hold if we hold long enough in corner then drag out.
TEST_F(SideSwipeDetectorTest, CornerHoldDragOutOfCorner) {
  EXPECT_CALL(test_gesture_handler(), HandledCornerHolds())
      .WillRepeatedly(Return(CastGestureHandler::TOP_LEFT_CORNER));
  EXPECT_CALL(test_gesture_handler(), HandleCornerHold(_, _)).Times(1);
  EXPECT_CALL(test_gesture_handler(), HandleCornerHoldEnd(_, _)).Times(1);
  Drag(kNWCorner, kHoldCornerDelay, kHoldCornerDelay,
       kNWCorner + gfx::Vector2d(100, 100));

  base::RunLoop().RunUntilIdle();
}

// Test that it's not a corner hold if it doesn't sit in the corner long enough.
TEST_F(SideSwipeDetectorTest, CornerHoldCancelNotLongEnough) {
  EXPECT_CALL(test_gesture_handler(), HandledCornerHolds())
      .WillRepeatedly(Return(CastGestureHandler::TOP_LEFT_CORNER));
  EXPECT_CALL(test_gesture_handler(), HandleCornerHold(_, _)).Times(0);
  EXPECT_CALL(test_gesture_handler(), HandleCornerHoldEnd(_, _)).Times(0);
  Hold(kNWCorner, base::TimeDelta::FromMilliseconds(5));

  base::RunLoop().RunUntilIdle();
}

// Test that it's not a corner hold if it drags out of the corner before the
// hold time.
TEST_F(SideSwipeDetectorTest, CornerHoldCancelDragOutOfCorner) {
  EXPECT_CALL(test_gesture_handler(), HandledCornerHolds())
      .WillRepeatedly(Return(CastGestureHandler::TOP_LEFT_CORNER));
  EXPECT_CALL(test_gesture_handler(), HandleCornerHold(_, _)).Times(0);
  EXPECT_CALL(test_gesture_handler(), HandleCornerHoldEnd(_, _)).Times(0);
  Drag(kNWCorner, base::TimeDelta::FromMilliseconds(5), kHoldCornerDelay,
       kNWCorner + gfx::Vector2d(100, 100));

  base::RunLoop().RunUntilIdle();
}

// Test that it's not a corner hold if it drags out of the corner before the
// hold time, even with the finger held.
TEST_F(SideSwipeDetectorTest, CornerHoldCancelDragOutOfCornerButHeld) {
  EXPECT_CALL(test_gesture_handler(), HandledCornerHolds())
      .WillRepeatedly(Return(CastGestureHandler::TOP_LEFT_CORNER));
  EXPECT_CALL(test_gesture_handler(), HandleCornerHold(_, _)).Times(0);
  EXPECT_CALL(test_gesture_handler(), HandleCornerHoldEnd(_, _)).Times(0);
  Drag(kNWCorner, base::TimeDelta::FromMilliseconds(5), kHoldCornerDelay,
       kNWCorner + gfx::Vector2d(100, 100), false /* end_release */);

  base::RunLoop().RunUntilIdle();
}

}  // namespace test
}  // namespace chromecast
