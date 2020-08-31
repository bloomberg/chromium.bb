// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/utility/screenshot_controller.h"

#include "ash/display/cursor_window_controller.h"
#include "ash/display/mirror_window_test_api.h"
#include "ash/display/mouse_cursor_event_filter.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/screenshot_delegate.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test_screenshot_delegate.h"
#include "ash/wm/window_util.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "ui/aura/env.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/cursor_size.h"
#include "ui/display/types/display_constants.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/snapshot/screenshot_grabber.h"
#include "ui/wm/core/cursor_manager.h"

namespace ash {

class ScreenshotControllerTest : public AshTestBase {
 public:
  ScreenshotControllerTest() = default;
  ~ScreenshotControllerTest() override = default;

 protected:
  ScreenshotController* screenshot_controller() {
    return Shell::Get()->screenshot_controller();
  }

  bool TestIfMouseWarpsAt(const gfx::Point& point_in_screen) {
    return AshTestBase::TestIfMouseWarpsAt(GetEventGenerator(),
                                           point_in_screen);
  }

  void StartPartialScreenshotSession() {
    screenshot_controller()->StartPartialScreenshotSession(true);
  }

  void StartWindowScreenshotSession() {
    screenshot_controller()->StartWindowScreenshotSession();
  }

  void Cancel() { screenshot_controller()->CancelScreenshotSession(); }

  bool IsActive() { return screenshot_controller()->in_screenshot_session_; }

  gfx::Point GetStartPosition() const {
    return Shell::Get()->screenshot_controller()->GetStartPositionForTest();
  }

  const aura::Window* GetCurrentSelectedWindow() const {
    return Shell::Get()->screenshot_controller()->selected_;
  }

  aura::Window* CreateSelectableWindow(const gfx::Rect& rect) {
    return CreateTestWindowInShell(SK_ColorGRAY, 0, rect);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenshotControllerTest);
};

using WindowScreenshotControllerTest = ScreenshotControllerTest;
using PartialScreenshotControllerTest = ScreenshotControllerTest;

TEST_F(PartialScreenshotControllerTest, BasicMouse) {
  StartPartialScreenshotSession();
  TestScreenshotDelegate* test_delegate = GetScreenshotDelegate();
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());

  generator.MoveMouseTo(100, 100);
  generator.PressLeftButton();
  EXPECT_EQ(gfx::Point(100, 100), GetStartPosition());
  EXPECT_EQ(0, test_delegate->handle_take_partial_screenshot_count());

  generator.MoveMouseTo(200, 200);
  EXPECT_EQ(0, test_delegate->handle_take_partial_screenshot_count());

  generator.ReleaseLeftButton();
  EXPECT_EQ(gfx::Rect(100, 100, 100, 100),
            GetScreenshotDelegate()->last_rect());
  EXPECT_EQ(1, GetScreenshotDelegate()->handle_take_partial_screenshot_count());

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsActive());
}

// Starting the screenshot session while mouse is pressed, releasing the mouse
// without moving it used to cause a crash. Make sure this doesn't happen again.
// crbug.com/581432.
TEST_F(PartialScreenshotControllerTest, StartSessionWhileMousePressed) {
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  TestScreenshotDelegate* test_delegate = GetScreenshotDelegate();

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
  TestScreenshotDelegate* test_delegate = GetScreenshotDelegate();
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());

  generator.MoveMouseTo(100, 100);

  // No moves, just clicking at the same position.
  generator.ClickLeftButton();
  EXPECT_EQ(0, test_delegate->handle_take_partial_screenshot_count());

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsActive());
}

TEST_F(PartialScreenshotControllerTest, Movable) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kMovablePartialScreenshot);

  TestScreenshotDelegate* test_delegate = GetScreenshotDelegate();
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());

  // Test data to exercise on an initial region of 10+20-50x50.
  struct {
    const gfx::Point drag_start;
    const gfx::Point drag_end;
    const gfx::Rect expected_region;
  } kTestCases[] = {
      // Drag from top-left. |drag_end| becomes the new origin.
      {gfx::Point(10, 20), gfx::Point(20, 30), gfx::Rect(20, 30, 40, 40)},
      // Drag from top-right. |drag_end| becomes the new top-right.
      {gfx::Point(60, 20), gfx::Point(20, 30), gfx::Rect(10, 30, 10, 40)},
      // Drag from bottom-left. |drag_end| becomes the new bottom-left.
      {gfx::Point(10, 70), gfx::Point(20, 30), gfx::Rect(20, 20, 40, 10)},
      // Drag from bottom-right. |drag_end| becomes the new bottom-right.
      {gfx::Point(60, 70), gfx::Point(20, 30), gfx::Rect(10, 20, 10, 10)},

      // Drag from top. |drag_end|'s y becomes new top.
      {gfx::Point(30, 20), gfx::Point(20, 30), gfx::Rect(10, 30, 50, 40)},
      // Drag from top. |drag_end|'s y becomes new bottom.
      {gfx::Point(30, 20), gfx::Point(20, 80), gfx::Rect(10, 70, 50, 10)},

      // Drag from left. |drag_end|'x becomes the new left.
      {gfx::Point(10, 30), gfx::Point(20, 30), gfx::Rect(20, 20, 40, 50)},
      // Drag from left. |drag_end|'x becomes the new right.
      {gfx::Point(10, 30), gfx::Point(70, 30), gfx::Rect(60, 20, 10, 50)},

      // Drag from right. |drag_end|'x becomes the new right.
      {gfx::Point(60, 30), gfx::Point(20, 30), gfx::Rect(10, 20, 10, 50)},
      // Drag from right. |drag_end|'x becomes the new left.
      {gfx::Point(60, 30), gfx::Point(5, 30), gfx::Rect(5, 20, 5, 50)},

      // Drag from bottom. |drag_end|'y becomes the new bottom.
      {gfx::Point(30, 70), gfx::Point(20, 30), gfx::Rect(10, 20, 50, 10)},
      // Drag from bottom. |drag_end|'y becomes the new top.
      {gfx::Point(30, 70), gfx::Point(20, 10), gfx::Rect(10, 10, 50, 10)},
  };
  for (size_t i = 0; i < base::size(kTestCases); ++i) {
    const auto& test = kTestCases[i];
    SCOPED_TRACE(testing::Message()
                 << ", i=" << i << "start=" << test.drag_start.ToString()
                 << ", end=" << test.drag_end.ToString()
                 << ", expect=" << test.expected_region.ToString());

    StartPartialScreenshotSession();
    generator.MoveMouseTo(10, 20);
    generator.DragMouseBy(50, 50);

    generator.MoveMouseTo(test.drag_start);
    generator.DragMouseTo(test.drag_end);

    // Complete partial screenshot selection by clicking outside the region.
    generator.MoveMouseTo(0, 0);
    generator.ClickLeftButton();

    EXPECT_EQ(test.expected_region, test_delegate->last_rect());
    EXPECT_EQ(static_cast<int>(i + 1),
              test_delegate->handle_take_partial_screenshot_count());
  }
}

TEST_F(PartialScreenshotControllerTest, BasicTouch) {
  StartPartialScreenshotSession();
  TestScreenshotDelegate* test_delegate = GetScreenshotDelegate();
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());

  generator.set_current_screen_location(gfx::Point(100, 100));
  generator.PressTouch();
  EXPECT_EQ(0, test_delegate->handle_take_partial_screenshot_count());
  EXPECT_EQ(gfx::Point(100, 100), GetStartPosition());

  generator.MoveTouch(gfx::Point(200, 200));
  EXPECT_EQ(0, test_delegate->handle_take_partial_screenshot_count());

  generator.ReleaseTouch();
  EXPECT_EQ(gfx::Rect(100, 100, 100, 100),
            GetScreenshotDelegate()->last_rect());
  EXPECT_EQ(1, GetScreenshotDelegate()->handle_take_partial_screenshot_count());

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsActive());
}

// Verifies that pointer events can be used to take a screenshot when
// pointer-only move is set to true. Verifies that pointer-only mode is
// automatically reset to false.
TEST_F(PartialScreenshotControllerTest,
       PointerEventsWorkWhenPointerOnlyActive) {
  StartPartialScreenshotSession();
  TestScreenshotDelegate* test_delegate = GetScreenshotDelegate();
  screenshot_controller()->set_pen_events_only(true);
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());

  generator.EnterPenPointerMode();
  generator.set_current_screen_location(gfx::Point(100, 100));
  generator.PressTouch();
  EXPECT_EQ(0, test_delegate->handle_take_partial_screenshot_count());
  EXPECT_EQ(gfx::Point(100, 100), GetStartPosition());

  generator.MoveTouch(gfx::Point(300, 300));
  EXPECT_EQ(0, test_delegate->handle_take_partial_screenshot_count());

  generator.ReleaseTouch();
  EXPECT_EQ(gfx::Rect(100, 100, 200, 200),
            GetScreenshotDelegate()->last_rect());
  EXPECT_EQ(1, GetScreenshotDelegate()->handle_take_partial_screenshot_count());
  EXPECT_FALSE(screenshot_controller()->pen_events_only());

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsActive());
}

// Verifies that only pointer press/release events can be used to take a
// screenshot when pointer only mode is active.
TEST_F(PartialScreenshotControllerTest,
       TouchMousePointerHoverIgnoredWithPointerEvents) {
  StartPartialScreenshotSession();
  screenshot_controller()->set_pen_events_only(true);
  TestScreenshotDelegate* test_delegate = GetScreenshotDelegate();
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  generator.set_current_screen_location(gfx::Point(100, 100));

  // Verify touch is ignored.
  generator.PressTouch();
  generator.MoveTouch(gfx::Point(50, 50));
  generator.ReleaseTouch();
  EXPECT_EQ(0, test_delegate->handle_take_partial_screenshot_count());
  EXPECT_EQ(gfx::Point(0, 0), GetStartPosition());

  // Verify mouse is ignored.
  generator.DragMouseBy(10, 10);
  EXPECT_EQ(0, test_delegate->handle_take_partial_screenshot_count());
  EXPECT_EQ(gfx::Point(0, 0), GetStartPosition());

  // Verify pointer enter/exit is ignored.
  generator.EnterPenPointerMode();
  generator.SendMouseEnter();
  generator.set_current_screen_location(gfx::Point(100, 100));
  generator.SendMouseExit();
  generator.ExitPenPointerMode();
  EXPECT_EQ(0, test_delegate->handle_take_partial_screenshot_count());
  EXPECT_EQ(gfx::Point(0, 0), GetStartPosition());

  Cancel();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsActive());
}

TEST_F(PartialScreenshotControllerTest, TwoFingerTouch) {
  StartPartialScreenshotSession();
  TestScreenshotDelegate* test_delegate = GetScreenshotDelegate();
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());

  generator.set_current_screen_location(gfx::Point(100, 100));
  generator.PressTouch();
  EXPECT_EQ(0, test_delegate->handle_take_partial_screenshot_count());
  EXPECT_EQ(gfx::Point(100, 100), GetStartPosition());

  generator.set_current_screen_location(gfx::Point(200, 200));
  generator.PressTouchId(1);
  EXPECT_EQ(gfx::Rect(100, 100, 100, 100),
            GetScreenshotDelegate()->last_rect());
  EXPECT_EQ(1, GetScreenshotDelegate()->handle_take_partial_screenshot_count());

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsActive());
}

// Make sure ScreenshotController doesn't allow taking screenshot
// across multiple monitors
// cursor. See http://crbug.com/462229
TEST_F(PartialScreenshotControllerTest, MouseWarpTest) {
  // Create two displays.
  Shell* shell = Shell::Get();
  UpdateDisplay("500x500,500x500");
  EXPECT_EQ(2U, shell->display_manager()->GetNumDisplays());

  StartPartialScreenshotSession();
  EXPECT_FALSE(TestIfMouseWarpsAt(gfx::Point(499, 11)));
  EXPECT_EQ(gfx::Point(499, 11),
            aura::Env::GetInstance()->last_mouse_location());

  Cancel();
  EXPECT_TRUE(TestIfMouseWarpsAt(gfx::Point(499, 11)));
  EXPECT_EQ(gfx::Point(501, 11),
            aura::Env::GetInstance()->last_mouse_location());
}

TEST_F(PartialScreenshotControllerTest, CursorVisibilityTest) {
  aura::client::CursorClient* client = Shell::Get()->cursor_manager();

  GetEventGenerator()->PressKey(ui::VKEY_A, 0);
  GetEventGenerator()->ReleaseKey(ui::VKEY_A, 0);

  EXPECT_FALSE(client->IsCursorVisible());

  StartPartialScreenshotSession();
  EXPECT_TRUE(IsActive());
  EXPECT_TRUE(client->IsCursorVisible());

  // Platform's Cursor should be hidden while dragging.
  GetEventGenerator()->PressLeftButton();
  EXPECT_TRUE(IsActive());
  EXPECT_FALSE(client->IsCursorVisible());

  Cancel();
  EXPECT_TRUE(client->IsCursorVisible());
}

// Make sure ScreenshotController doesn't prevent handling of large
// cursor. See http://crbug.com/459214
TEST_F(PartialScreenshotControllerTest, LargeCursor) {
  Shell::Get()->cursor_manager()->SetCursorSize(ui::CursorSize::kLarge);
  Shell::Get()
      ->window_tree_host_manager()
      ->cursor_window_controller()
      ->SetCursorCompositingEnabled(true);

  // Large cursor is represented as cursor window.
  MirrorWindowTestApi test_api;
  ASSERT_NE(nullptr, test_api.GetCursorWindow());

  ui::test::EventGenerator event_generator(Shell::GetPrimaryRootWindow());
  gfx::Point cursor_location;
  event_generator.MoveMouseTo(cursor_location);
  EXPECT_EQ(cursor_location, test_api.GetCursorLocation());

  StartPartialScreenshotSession();
  EXPECT_TRUE(IsActive());

  cursor_location += gfx::Vector2d(1, 1);
  event_generator.MoveMouseTo(cursor_location);
  EXPECT_EQ(cursor_location, test_api.GetCursorLocation());

  event_generator.PressLeftButton();
  cursor_location += gfx::Vector2d(5, 5);
  event_generator.MoveMouseTo(cursor_location);
  EXPECT_EQ(cursor_location, test_api.GetCursorLocation());

  event_generator.ReleaseLeftButton();

  EXPECT_EQ(1, GetScreenshotDelegate()->handle_take_partial_screenshot_count());
  EXPECT_EQ(gfx::Rect(1, 1, 5, 5), GetScreenshotDelegate()->last_rect());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsActive());
}

TEST_F(WindowScreenshotControllerTest, KeyboardOperation) {
  ui::test::EventGenerator* generator = GetEventGenerator();
  TestScreenshotDelegate* test_delegate = GetScreenshotDelegate();

  StartWindowScreenshotSession();
  generator->PressKey(ui::VKEY_ESCAPE, 0);
  generator->ReleaseKey(ui::VKEY_ESCAPE, 0);
  EXPECT_FALSE(IsActive());
  EXPECT_FALSE(test_delegate->GetSelectedWindowAndReset());

  StartWindowScreenshotSession();
  generator->PressKey(ui::VKEY_RETURN, 0);
  generator->ReleaseKey(ui::VKEY_RETURN, 0);
  EXPECT_FALSE(IsActive());
  EXPECT_FALSE(test_delegate->GetSelectedWindowAndReset());

  std::unique_ptr<aura::Window> window1(
      CreateSelectableWindow(gfx::Rect(5, 5, 20, 20)));
  wm::ActivateWindow(window1.get());
  StartWindowScreenshotSession();
  generator->PressKey(ui::VKEY_ESCAPE, 0);
  generator->ReleaseKey(ui::VKEY_ESCAPE, 0);
  EXPECT_FALSE(IsActive());
  EXPECT_FALSE(test_delegate->GetSelectedWindowAndReset());

  StartWindowScreenshotSession();
  generator->PressKey(ui::VKEY_RETURN, 0);
  generator->ReleaseKey(ui::VKEY_RETURN, 0);
  EXPECT_FALSE(IsActive());
  EXPECT_EQ(window1.get(), test_delegate->GetSelectedWindowAndReset());
  // Make sure it's properly reset.
  EXPECT_FALSE(test_delegate->GetSelectedWindowAndReset());
}

TEST_F(WindowScreenshotControllerTest, MouseOperation) {
  ui::test::EventGenerator* generator = GetEventGenerator();
  TestScreenshotDelegate* test_delegate = GetScreenshotDelegate();
  StartWindowScreenshotSession();
  EXPECT_TRUE(IsActive());
  generator->ClickLeftButton();
  EXPECT_FALSE(IsActive());
  EXPECT_FALSE(test_delegate->GetSelectedWindowAndReset());

  std::unique_ptr<aura::Window> window1(
      CreateSelectableWindow(gfx::Rect(5, 5, 20, 20)));
  std::unique_ptr<aura::Window> window2(
      CreateSelectableWindow(gfx::Rect(100, 100, 100, 100)));
  wm::ActivateWindow(window1.get());
  StartWindowScreenshotSession();
  EXPECT_EQ(window1.get(), GetCurrentSelectedWindow());
  generator->MoveMouseTo(150, 150);
  EXPECT_EQ(window2.get(), GetCurrentSelectedWindow());
  generator->MoveMouseTo(400, 0);
  EXPECT_FALSE(GetCurrentSelectedWindow());
  generator->MoveMouseTo(10, 10);
  EXPECT_EQ(window1.get(), GetCurrentSelectedWindow());
  generator->ClickLeftButton();
  EXPECT_EQ(window1.get(), test_delegate->GetSelectedWindowAndReset());

  // Window selection should work even with Capture.
  window2->SetCapture();
  wm::ActivateWindow(window2.get());
  StartWindowScreenshotSession();
  EXPECT_EQ(window2.get(), GetCurrentSelectedWindow());
  generator->MoveMouseTo(10, 10);
  EXPECT_EQ(window1.get(), GetCurrentSelectedWindow());
  generator->MoveMouseTo(400, 0);
  EXPECT_FALSE(GetCurrentSelectedWindow());
  generator->MoveMouseTo(10, 10);
  EXPECT_EQ(window1.get(), GetCurrentSelectedWindow());
  generator->ClickLeftButton();
  EXPECT_EQ(window1.get(), test_delegate->GetSelectedWindowAndReset());

  // Remove window.
  StartWindowScreenshotSession();
  generator->MoveMouseTo(10, 10);
  EXPECT_EQ(window1.get(), GetCurrentSelectedWindow());
  window1.reset();
  EXPECT_FALSE(GetCurrentSelectedWindow());
  generator->ClickLeftButton();
  EXPECT_FALSE(test_delegate->GetSelectedWindowAndReset());
}

TEST_F(WindowScreenshotControllerTest, MultiDisplays) {
  UpdateDisplay("400x400,500x500");

  ui::test::EventGenerator* generator = GetEventGenerator();
  TestScreenshotDelegate* test_delegate = GetScreenshotDelegate();

  std::unique_ptr<aura::Window> window1(
      CreateSelectableWindow(gfx::Rect(100, 100, 100, 100)));
  std::unique_ptr<aura::Window> window2(
      CreateSelectableWindow(gfx::Rect(600, 200, 100, 100)));
  EXPECT_NE(window1.get()->GetRootWindow(), window2.get()->GetRootWindow());

  StartWindowScreenshotSession();
  generator->MoveMouseTo(150, 150);
  EXPECT_EQ(window1.get(), GetCurrentSelectedWindow());
  generator->MoveMouseTo(650, 250);
  EXPECT_EQ(window2.get(), GetCurrentSelectedWindow());
  generator->ClickLeftButton();
  EXPECT_EQ(window2.get(), test_delegate->GetSelectedWindowAndReset());

  window2->SetCapture();
  wm::ActivateWindow(window2.get());
  StartWindowScreenshotSession();
  generator->MoveMouseTo(150, 150);
  EXPECT_EQ(window1.get(), GetCurrentSelectedWindow());
  generator->ClickLeftButton();
  EXPECT_EQ(window1.get(), test_delegate->GetSelectedWindowAndReset());
}

TEST_F(ScreenshotControllerTest, FractionScaleWithProperRounding) {
  UpdateDisplay(base::StringPrintf("3000x2000*%s", display::kDsfStr_2_252));
  aura::Window* root = Shell::GetAllRootWindows()[0];
  EXPECT_EQ(gfx::Size(1332, 888), root->bounds().size());
  EXPECT_EQ(display::kDsf_2_252, root->layer()->device_scale_factor());
  std::unique_ptr<ui::ScreenshotGrabber> grabber =
      std::make_unique<ui::ScreenshotGrabber>();

  auto callback = [](base::RunLoop* run_loop,
                     ui::ScreenshotResult screenshot_result,
                     scoped_refptr<base::RefCountedMemory> png_data) {
    ASSERT_TRUE(screenshot_result == ui::ScreenshotResult::SUCCESS);

    const unsigned char* input =
        reinterpret_cast<const unsigned char*>(png_data->front());
    size_t size = png_data->size();
    SkBitmap bitmap;
    ASSERT_TRUE(gfx::PNGCodec::Decode(input, size, &bitmap));
    EXPECT_EQ(bitmap.width(), 3000);
    EXPECT_EQ(bitmap.height(), 2000);
    run_loop->Quit();
  };
  base::RunLoop run_loop;
  ui::ScreenshotGrabber::ScreenshotCallback cb =
      base::BindOnce(callback, &run_loop);
  grabber->TakeScreenshot(root, gfx::Rect(root->bounds().size()),
                          std::move(cb));
  run_loop.Run();
}

TEST_F(ScreenshotControllerTest, MultipleDisplays) {
  StartPartialScreenshotSession();
  EXPECT_TRUE(IsActive());
  UpdateDisplay("400x400,500x500");
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsActive());

  StartPartialScreenshotSession();
  EXPECT_TRUE(IsActive());
  UpdateDisplay("400x400");
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsActive());

  StartWindowScreenshotSession();
  EXPECT_TRUE(IsActive());
  UpdateDisplay("400x400,500x500");
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsActive());

  StartWindowScreenshotSession();
  EXPECT_TRUE(IsActive());
  UpdateDisplay("400x400");
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsActive());
}

// Windows that take capture can misbehave due to a screenshot session. Break
// mouse capture when the screenshot session is over. See crbug.com/651939
TEST_F(ScreenshotControllerTest, BreaksCapture) {
  std::unique_ptr<aura::Window> window(
      CreateSelectableWindow(gfx::Rect(100, 100, 100, 100)));
  window->SetCapture();
  EXPECT_TRUE(window->HasCapture());
  StartWindowScreenshotSession();
  EXPECT_TRUE(window->HasCapture());
  Cancel();
  EXPECT_FALSE(window->HasCapture());
}

}  // namespace ash
