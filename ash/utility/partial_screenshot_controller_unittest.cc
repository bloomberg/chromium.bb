// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/utility/partial_screenshot_controller.h"

#include "ash/screenshot_delegate.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_screenshot_delegate.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/events/test/event_generator.h"

namespace ash {

class PartialScreenshotControllerTest : public test::AshTestBase {
 public:
  PartialScreenshotControllerTest() {}
  ~PartialScreenshotControllerTest() override {}

  void SetUp() override {
    test::AshTestBase::SetUp();
    PartialScreenshotController::StartPartialScreenshotSession(
        GetScreenshotDelegate());
  }

 protected:
  static PartialScreenshotController* GetInstance() {
    return PartialScreenshotController::GetInstanceForTest();
  }

  const gfx::Point& GetStartPosition() const {
    return GetInstance()->start_position_;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PartialScreenshotControllerTest);
};

TEST_F(PartialScreenshotControllerTest, BasicMouse) {
  test::TestScreenshotDelegate* test_delegate = GetScreenshotDelegate();
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());

  generator.MoveMouseTo(100, 100);
  generator.PressLeftButton();
  EXPECT_EQ("100,100", GetStartPosition().ToString());
  EXPECT_EQ(0, test_delegate->handle_take_partial_screenshot_count());

  generator.MoveMouseTo(200, 200);
  EXPECT_EQ(0, test_delegate->handle_take_partial_screenshot_count());

  generator.ReleaseLeftButton();
  EXPECT_EQ("100,100 100x100", GetScreenshotDelegate()->last_rect().ToString());
  EXPECT_EQ(1, GetScreenshotDelegate()->handle_take_partial_screenshot_count());

  RunAllPendingInMessageLoop();
  EXPECT_EQ(nullptr, GetInstance());
}

TEST_F(PartialScreenshotControllerTest, JustClick) {
  test::TestScreenshotDelegate* test_delegate = GetScreenshotDelegate();
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());

  generator.MoveMouseTo(100, 100);

  // No moves, just clicking at the same position.
  generator.ClickLeftButton();
  EXPECT_EQ(0, test_delegate->handle_take_partial_screenshot_count());

  RunAllPendingInMessageLoop();
  EXPECT_EQ(nullptr, GetInstance());
}

TEST_F(PartialScreenshotControllerTest, BasicTouch) {
  test::TestScreenshotDelegate* test_delegate = GetScreenshotDelegate();
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());

  generator.set_current_location(gfx::Point(100, 100));
  generator.PressTouch();
  EXPECT_EQ(0, test_delegate->handle_take_partial_screenshot_count());
  EXPECT_EQ("100,100", GetStartPosition().ToString());

  generator.MoveTouch(gfx::Point(200, 200));
  EXPECT_EQ(0, test_delegate->handle_take_partial_screenshot_count());

  generator.ReleaseTouch();
  EXPECT_EQ("100,100 100x100", GetScreenshotDelegate()->last_rect().ToString());
  EXPECT_EQ(1, GetScreenshotDelegate()->handle_take_partial_screenshot_count());

  RunAllPendingInMessageLoop();
  EXPECT_EQ(nullptr, GetInstance());
}

TEST_F(PartialScreenshotControllerTest, TwoFingerTouch) {
  test::TestScreenshotDelegate* test_delegate = GetScreenshotDelegate();
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());

  generator.set_current_location(gfx::Point(100, 100));
  generator.PressTouch();
  EXPECT_EQ(0, test_delegate->handle_take_partial_screenshot_count());
  EXPECT_EQ("100,100", GetStartPosition().ToString());

  generator.set_current_location(gfx::Point(200, 200));
  generator.PressTouchId(1);
  EXPECT_EQ("100,100 100x100", GetScreenshotDelegate()->last_rect().ToString());
  EXPECT_EQ(1, GetScreenshotDelegate()->handle_take_partial_screenshot_count());

  RunAllPendingInMessageLoop();
  EXPECT_EQ(nullptr, GetInstance());
}

TEST_F(PartialScreenshotControllerTest, DontStartTwice) {
  PartialScreenshotController* controller = GetInstance();

  PartialScreenshotController::StartPartialScreenshotSession(
      GetScreenshotDelegate());

  // The same instance.
  EXPECT_EQ(controller, GetInstance());
}

TEST_F(PartialScreenshotControllerTest, MultipleDisplays) {
  if (!SupportsMultipleDisplays())
    return;

  EXPECT_NE(nullptr, GetInstance());
  UpdateDisplay("400x400,500x500");
  RunAllPendingInMessageLoop();
  EXPECT_EQ(nullptr, GetInstance());

  PartialScreenshotController::StartPartialScreenshotSession(
      GetScreenshotDelegate());
  EXPECT_NE(nullptr, GetInstance());
  UpdateDisplay("400x400");
  RunAllPendingInMessageLoop();
  EXPECT_EQ(nullptr, GetInstance());
}

}  // namespace ash
