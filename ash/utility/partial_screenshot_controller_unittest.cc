// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/utility/partial_screenshot_controller.h"

#include "ash/display/cursor_window_controller.h"
#include "ash/display/mouse_cursor_event_filter.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/screenshot_delegate.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/display_manager_test_api.h"
#include "ash/test/mirror_window_test_api.h"
#include "ash/test/test_screenshot_delegate.h"
#include "ui/aura/env.h"
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

  bool TestIfMouseWarpsAt(const gfx::Point& point_in_screen) {
    return test::DisplayManagerTestApi::TestIfMouseWarpsAt(GetEventGenerator(),
                                                           point_in_screen);
  }

  void StartPartialScreenshotSession() {
    partial_screenshot_controller()->StartPartialScreenshotSession(
        GetScreenshotDelegate());
  }

  void Cancel() { partial_screenshot_controller()->Cancel(); }

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

// Starting the screenshot session while mouse is pressed, releasing the mouse
// without moving it used to cause a crash. Make sure this doesn't happen again.
// crbug.com/581432.
TEST_F(PartialScreenshotControllerTest, StartSessionWhileMousePressed) {
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  test::TestScreenshotDelegate* test_delegate = GetScreenshotDelegate();

  generator.MoveMouseTo(100, 100);
  generator.PressLeftButton();

  // The following used to cause a crash. Now it should remain in the
  // screenshot session.
  StartPartialScreenshotSession();
  generator.ReleaseLeftButton();
  EXPECT_EQ(0, test_delegate->handle_take_partial_screenshot_count());
  EXPECT_TRUE(IsActive());

  // Pressing again, moving, and releasing should take a screenshot.
  generator.PressLeftButton();
  generator.MoveMouseTo(200, 200);
  generator.ReleaseLeftButton();
  EXPECT_EQ(1, test_delegate->handle_take_partial_screenshot_count());
  EXPECT_FALSE(IsActive());

  // Starting the screenshot session while mouse is pressed, moving the mouse
  // and releasing should take a screenshot normally.
  generator.MoveMouseTo(100, 100);
  generator.PressLeftButton();
  StartPartialScreenshotSession();
  generator.MoveMouseTo(150, 150);
  generator.MoveMouseTo(200, 200);
  EXPECT_TRUE(IsActive());
  generator.ReleaseLeftButton();
  EXPECT_EQ(2, test_delegate->handle_take_partial_screenshot_count());
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

// Make sure PartialScreenshotController doesn't allow taking screenshot
// across multiple monitors
// cursor. See http://crbug.com/462229
#if defined(OS_CHROMEOS)
TEST_F(PartialScreenshotControllerTest, MouseWarpTest) {
  if (!SupportsMultipleDisplays())
    return;

  // Create two displays.
  Shell* shell = Shell::GetInstance();
  UpdateDisplay("500x500,500x500");
  EXPECT_EQ(2U, shell->display_manager()->GetNumDisplays());

  StartPartialScreenshotSession();
  EXPECT_FALSE(TestIfMouseWarpsAt(gfx::Point(499, 11)));
  EXPECT_EQ("499,11",
            aura::Env::GetInstance()->last_mouse_location().ToString());

  Cancel();
  EXPECT_TRUE(TestIfMouseWarpsAt(gfx::Point(499, 11)));
  EXPECT_EQ("501,11",
            aura::Env::GetInstance()->last_mouse_location().ToString());
}

TEST_F(PartialScreenshotControllerTest, VisibilityTest) {
  aura::client::CursorClient* client = Shell::GetInstance()->cursor_manager();

  GetEventGenerator().PressKey(ui::VKEY_A, 0);
  GetEventGenerator().ReleaseKey(ui::VKEY_A, 0);

  EXPECT_FALSE(client->IsCursorVisible());

  StartPartialScreenshotSession();
  EXPECT_TRUE(IsActive());
  EXPECT_TRUE(client->IsCursorVisible());

  Cancel();
  EXPECT_TRUE(client->IsCursorVisible());
}

// Make sure PartialScreenshotController doesn't prevent handling of large
// cursor. See http://crbug.com/459214
TEST_F(PartialScreenshotControllerTest, LargeCursor) {
  Shell::GetInstance()->cursor_manager()->SetCursorSet(ui::CURSOR_SET_LARGE);
  Shell::GetInstance()
      ->window_tree_host_manager()
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
