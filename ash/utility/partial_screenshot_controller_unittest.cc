// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/utility/partial_screenshot_controller.h"

#include "ash/display/cursor_window_controller.h"
#include "ash/display/display_controller.h"
#include "ash/screenshot_delegate.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/mirror_window_test_api.h"
#include "ash/test/test_screenshot_delegate.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/cursor/cursor.h"
#include "ui/events/test/event_generator.h"
#include "ui/wm/core/cursor_manager.h"

namespace ash {

class PartialScreenshotControllerTest : public test::AshTestBase {
 public:
  PartialScreenshotControllerTest() {}
  ~PartialScreenshotControllerTest() override {}

 protected:
  PartialScreenshotController* partial_screenshot_controller() {
    return Shell::GetInstance()->partial_screenshot_controller();
  }

  void StartPartialScreenshotSession() {
    partial_screenshot_controller()->StartPartialScreenshotSession(
        GetScreenshotDelegate());
  }

  bool IsActive() {
    return partial_screenshot_controller()->screenshot_delegate_ != nullptr;
  }

  const gfx::Point& GetStartPosition() const {
    return Shell::GetInstance()
        ->partial_screenshot_controller()
        ->start_position_;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PartialScreenshotControllerTest);
};

TEST_F(PartialScreenshotControllerTest, BasicMouse) {
  StartPartialScreenshotSession();
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
  EXPECT_FALSE(IsActive());
}

TEST_F(PartialScreenshotControllerTest, JustClick) {
  StartPartialScreenshotSession();
  test::TestScreenshotDelegate* test_delegate = GetScreenshotDelegate();
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());

  generator.MoveMouseTo(100, 100);

  // No moves, just clicking at the same position.
  generator.ClickLeftButton();
  EXPECT_EQ(0, test_delegate->handle_take_partial_screenshot_count());

  RunAllPendingInMessageLoop();
  EXPECT_FALSE(IsActive());
}

TEST_F(PartialScreenshotControllerTest, BasicTouch) {
  StartPartialScreenshotSession();
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
  EXPECT_FALSE(IsActive());
}

TEST_F(PartialScreenshotControllerTest, TwoFingerTouch) {
  StartPartialScreenshotSession();
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
  EXPECT_FALSE(IsActive());
}

TEST_F(PartialScreenshotControllerTest, MultipleDisplays) {
  if (!SupportsMultipleDisplays())
    return;

  StartPartialScreenshotSession();
  EXPECT_TRUE(IsActive());
  UpdateDisplay("400x400,500x500");
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(IsActive());

  StartPartialScreenshotSession();
  EXPECT_TRUE(IsActive());
  UpdateDisplay("400x400");
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(IsActive());
}

#if defined(OS_CHROMEOS)
// Make sure PartialScreenshotController doesn't prevent handling of large
// cursor. See http://crbug.com/459214
TEST_F(PartialScreenshotControllerTest, LargeCursor) {
  Shell::GetInstance()->cursor_manager()->SetCursorSet(ui::CURSOR_SET_LARGE);
  Shell::GetInstance()
      ->display_controller()
      ->cursor_window_controller()
      ->SetCursorCompositingEnabled(true);

  // Large cursor is represented as cursor window.
  test::MirrorWindowTestApi test_api;
  ASSERT_NE(nullptr, test_api.GetCursorWindow());

  ui::test::EventGenerator event_generator(Shell::GetPrimaryRootWindow());
  gfx::Point cursor_location;
  event_generator.MoveMouseTo(cursor_location);
  EXPECT_EQ(cursor_location.ToString(),
            test_api.GetCursorLocation().ToString());

  StartPartialScreenshotSession();
  EXPECT_TRUE(IsActive());

  cursor_location += gfx::Vector2d(1, 1);
  event_generator.MoveMouseTo(cursor_location);
  EXPECT_EQ(cursor_location.ToString(),
            test_api.GetCursorLocation().ToString());

  event_generator.PressLeftButton();
  cursor_location += gfx::Vector2d(5, 5);
  event_generator.MoveMouseTo(cursor_location);
  EXPECT_EQ(cursor_location.ToString(),
            test_api.GetCursorLocation().ToString());

  event_generator.ReleaseLeftButton();

  EXPECT_EQ(1, GetScreenshotDelegate()->handle_take_partial_screenshot_count());
  EXPECT_EQ("1,1 5x5", GetScreenshotDelegate()->last_rect().ToString());
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(IsActive());
}
#endif

}  // namespace ash
