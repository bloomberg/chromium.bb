// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/graphics/cast_system_gesture_event_handler.h"

#include <memory>

#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/event_generator_delegate_aura.h"
#include "ui/aura/window.h"
#include "ui/events/test/event_generator.h"
#include "ui/wm/core/default_screen_position_client.h"

namespace chromecast {
namespace test {

namespace {

constexpr base::TimeDelta kTimeDelay = base::TimeDelta::FromMilliseconds(100);
constexpr int kSwipeDistance = 50;

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

class TestSideSwipeGestureHandler
    : public CastSideSwipeGestureHandlerInterface {
 public:
  ~TestSideSwipeGestureHandler() override = default;

  void OnSideSwipeBegin(CastSideSwipeOrigin swipe_origin,
                        ui::GestureEvent* gesture_event) override {
    if (handle_swipe_) {
      begin_swipe_origin_ = swipe_origin;
      begin_gesture_event_ = gesture_event;

      gesture_event->SetHandled();
    }
  }

  void OnSideSwipeEnd(CastSideSwipeOrigin swipe_origin,
                      ui::GestureEvent* gesture_event) override {
    end_swipe_origin_ = swipe_origin;
    end_gesture_event_ = gesture_event;
  }

  void SetHandleSwipe(bool handle_swipe) { handle_swipe_ = handle_swipe; }

  CastSideSwipeOrigin begin_swipe_origin() const { return begin_swipe_origin_; }
  ui::GestureEvent* begin_gesture_event() const { return begin_gesture_event_; }

  CastSideSwipeOrigin end_swipe_origin() const { return end_swipe_origin_; }
  ui::GestureEvent* end_gesture_event() const { return end_gesture_event_; }

 private:
  bool handle_swipe_ = true;

  CastSideSwipeOrigin begin_swipe_origin_ = CastSideSwipeOrigin::NONE;
  ui::GestureEvent* begin_gesture_event_ = nullptr;

  CastSideSwipeOrigin end_swipe_origin_ = CastSideSwipeOrigin::NONE;
  ui::GestureEvent* end_gesture_event_ = nullptr;
};

class CastSystemGestureEventHandlerTest : public aura::test::AuraTestBase {
 public:
  ~CastSystemGestureEventHandlerTest() override = default;

  void SetUp() override {
    aura::test::AuraTestBase::SetUp();

    screen_position_client_.reset(new wm::DefaultScreenPositionClient());
    aura::client::SetScreenPositionClient(root_window(),
                                          screen_position_client_.get());

    gesture_event_handler_.reset(
        new CastSystemGestureEventHandler(root_window()));
    gesture_handler_.reset(new TestSideSwipeGestureHandler);
    gesture_event_handler_->AddSideSwipeGestureHandler(gesture_handler_.get());
  }

  void TearDown() override {
    gesture_event_handler_->RemoveSideSwipeGestureHandler(
        gesture_handler_.get());
    gesture_event_handler_.reset();
    gesture_handler_.reset();

    aura::test::AuraTestBase::TearDown();
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

 private:
  std::unique_ptr<aura::client::ScreenPositionClient> screen_position_client_;
  std::unique_ptr<ui::test::EventGenerator> event_generator_;

  std::unique_ptr<CastSystemGestureEventHandler> gesture_event_handler_;
  std::unique_ptr<TestSideSwipeGestureHandler> gesture_handler_;
};

// Test that initialization works and initial state is clean.
TEST_F(CastSystemGestureEventHandlerTest, Initialization) {
  EXPECT_EQ(CastSideSwipeOrigin::NONE,
            test_gesture_handler().begin_swipe_origin());
  EXPECT_EQ(nullptr, test_gesture_handler().begin_gesture_event());
  EXPECT_EQ(CastSideSwipeOrigin::NONE,
            test_gesture_handler().end_swipe_origin());
  EXPECT_EQ(nullptr, test_gesture_handler().end_gesture_event());
}

// A swipe in the middle of the screen should produce no system gesture.
TEST_F(CastSystemGestureEventHandlerTest, SwipeWithNoSystemGesture) {
  gfx::Point drag_point(root_window()->bounds().width() / 2,
                        root_window()->bounds().height() / 2);
  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.GestureScrollSequence(
      drag_point, drag_point - gfx::Vector2d(0, kSwipeDistance), kTimeDelay, 5);
  RunAllPendingInMessageLoop();

  EXPECT_EQ(CastSideSwipeOrigin::NONE,
            test_gesture_handler().begin_swipe_origin());
  EXPECT_EQ(nullptr, test_gesture_handler().begin_gesture_event());
  EXPECT_EQ(CastSideSwipeOrigin::NONE,
            test_gesture_handler().end_swipe_origin());
  EXPECT_EQ(nullptr, test_gesture_handler().end_gesture_event());
}

TEST_F(CastSystemGestureEventHandlerTest, SwipeFromLeft) {
  gfx::Point drag_point(0, root_window()->bounds().height() / 2);
  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.GestureScrollSequence(
      drag_point, drag_point + gfx::Vector2d(kSwipeDistance, 0), kTimeDelay, 5);
  RunAllPendingInMessageLoop();

  EXPECT_EQ(CastSideSwipeOrigin::LEFT,
            test_gesture_handler().begin_swipe_origin());
  EXPECT_NE(nullptr, test_gesture_handler().begin_gesture_event());
  EXPECT_EQ(CastSideSwipeOrigin::LEFT,
            test_gesture_handler().end_swipe_origin());
  EXPECT_NE(nullptr, test_gesture_handler().end_gesture_event());
}

TEST_F(CastSystemGestureEventHandlerTest, SwipeFromRight) {
  gfx::Point drag_point(root_window()->bounds().width(),
                        root_window()->bounds().height() / 2);
  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.GestureScrollSequence(
      drag_point, drag_point - gfx::Vector2d(kSwipeDistance, 0), kTimeDelay, 5);
  RunAllPendingInMessageLoop();

  EXPECT_EQ(CastSideSwipeOrigin::RIGHT,
            test_gesture_handler().begin_swipe_origin());
  EXPECT_NE(nullptr, test_gesture_handler().begin_gesture_event());
  EXPECT_EQ(CastSideSwipeOrigin::RIGHT,
            test_gesture_handler().end_swipe_origin());
  EXPECT_NE(nullptr, test_gesture_handler().end_gesture_event());
}

TEST_F(CastSystemGestureEventHandlerTest, SwipeFromTop) {
  gfx::Point drag_point(root_window()->bounds().width() / 2, 0);
  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.GestureScrollSequence(
      drag_point, drag_point + gfx::Vector2d(0, kSwipeDistance), kTimeDelay, 5);
  RunAllPendingInMessageLoop();

  EXPECT_EQ(CastSideSwipeOrigin::TOP,
            test_gesture_handler().begin_swipe_origin());
  EXPECT_NE(nullptr, test_gesture_handler().begin_gesture_event());
  EXPECT_EQ(CastSideSwipeOrigin::TOP,
            test_gesture_handler().end_swipe_origin());
  EXPECT_NE(nullptr, test_gesture_handler().end_gesture_event());
}

TEST_F(CastSystemGestureEventHandlerTest, SwipeFromBottom) {
  gfx::Point drag_point(root_window()->bounds().width() / 2,
                        root_window()->bounds().height());
  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.GestureScrollSequence(
      drag_point, drag_point - gfx::Vector2d(0, kSwipeDistance), kTimeDelay, 5);
  RunAllPendingInMessageLoop();

  EXPECT_EQ(CastSideSwipeOrigin::BOTTOM,
            test_gesture_handler().begin_swipe_origin());
  EXPECT_NE(nullptr, test_gesture_handler().begin_gesture_event());
  EXPECT_EQ(CastSideSwipeOrigin::BOTTOM,
            test_gesture_handler().end_swipe_origin());
  EXPECT_NE(nullptr, test_gesture_handler().end_gesture_event());
}

// Test that ignoring the gesture at its beginning will make it so the swipe
// is not produced at the end.
TEST_F(CastSystemGestureEventHandlerTest, SwipeUnhandledIgnored) {
  test_gesture_handler().SetHandleSwipe(false);

  gfx::Point drag_point(root_window()->bounds().width() / 2,
                        root_window()->bounds().height());
  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.GestureScrollSequence(
      drag_point, drag_point - gfx::Vector2d(0, kSwipeDistance), kTimeDelay, 5);
  RunAllPendingInMessageLoop();

  EXPECT_EQ(CastSideSwipeOrigin::NONE,
            test_gesture_handler().begin_swipe_origin());
  EXPECT_EQ(nullptr, test_gesture_handler().begin_gesture_event());
  EXPECT_EQ(CastSideSwipeOrigin::NONE,
            test_gesture_handler().end_swipe_origin());
  EXPECT_EQ(nullptr, test_gesture_handler().end_gesture_event());
}

}  // namespace test
}  // namespace chromecast
