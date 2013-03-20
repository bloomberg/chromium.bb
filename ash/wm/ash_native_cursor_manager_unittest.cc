// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/ash_native_cursor_manager.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/cursor_manager_test_api.h"
#include "ash/wm/image_cursors.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/gfx/screen.h"

using views::corewm::CursorManager;

namespace ash {
namespace test {

namespace {

// A delegate for recording a mouse event location.
class MouseEventLocationDelegate : public aura::test::TestWindowDelegate {
 public:
  MouseEventLocationDelegate() {}
  virtual ~MouseEventLocationDelegate() {}

  gfx::Point GetMouseEventLocationAndReset() {
    gfx::Point p = mouse_event_location_;
    mouse_event_location_.SetPoint(-100, -100);
    return p;
  }

  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE {
    mouse_event_location_ = event->location();
    event->SetHandled();
  }

 private:
  gfx::Point mouse_event_location_;

  DISALLOW_COPY_AND_ASSIGN(MouseEventLocationDelegate);
};

}  // namespace

typedef test::AshTestBase AshNativeCursorManagerTest;

TEST_F(AshNativeCursorManagerTest, LockCursor) {
  CursorManager* cursor_manager = Shell::GetInstance()->cursor_manager();
  CursorManagerTestApi test_api(cursor_manager);
  gfx::Display display(0);
#if defined(OS_WIN)
  cursor_manager->SetCursorResourceModule(L"ash_unittests.exe");
#endif
  cursor_manager->SetCursor(ui::kCursorCopy);
  EXPECT_EQ(ui::kCursorCopy, test_api.GetCurrentCursor().native_type());
  display.set_device_scale_factor(2.0f);
  display.set_rotation(gfx::Display::ROTATE_90);
  cursor_manager->SetDisplay(display);
  EXPECT_EQ(2.0f, test_api.GetDisplay().device_scale_factor());
  EXPECT_EQ(gfx::Display::ROTATE_90, test_api.GetDisplay().rotation());
  EXPECT_TRUE(test_api.GetCurrentCursor().platform());

  cursor_manager->LockCursor();
  EXPECT_TRUE(cursor_manager->is_cursor_locked());

  // Cursor type does not change while cursor is locked.
  cursor_manager->SetCursor(ui::kCursorPointer);
  EXPECT_EQ(ui::kCursorCopy, test_api.GetCurrentCursor().native_type());

  // Device scale factor and rotation do change even while cursor is locked.
  display.set_device_scale_factor(1.0f);
  display.set_rotation(gfx::Display::ROTATE_180);
  cursor_manager->SetDisplay(display);
  EXPECT_EQ(1.0f, test_api.GetDisplay().device_scale_factor());
  EXPECT_EQ(gfx::Display::ROTATE_180, test_api.GetDisplay().rotation());

  cursor_manager->UnlockCursor();
  EXPECT_FALSE(cursor_manager->is_cursor_locked());

  // Cursor type changes to the one specified while cursor is locked.
  EXPECT_EQ(ui::kCursorPointer, test_api.GetCurrentCursor().native_type());
  EXPECT_EQ(1.0f, test_api.GetDisplay().device_scale_factor());
  EXPECT_TRUE(test_api.GetCurrentCursor().platform());
}

TEST_F(AshNativeCursorManagerTest, SetCursor) {
  CursorManager* cursor_manager = Shell::GetInstance()->cursor_manager();
  CursorManagerTestApi test_api(cursor_manager);
#if defined(OS_WIN)
  cursor_manager->SetCursorResourceModule(L"ash_unittests.exe");
#endif
  cursor_manager->SetCursor(ui::kCursorCopy);
  EXPECT_EQ(ui::kCursorCopy, test_api.GetCurrentCursor().native_type());
  EXPECT_TRUE(test_api.GetCurrentCursor().platform());
  cursor_manager->SetCursor(ui::kCursorPointer);
  EXPECT_EQ(ui::kCursorPointer, test_api.GetCurrentCursor().native_type());
  EXPECT_TRUE(test_api.GetCurrentCursor().platform());
}

TEST_F(AshNativeCursorManagerTest, SetDeviceScaleFactorAndRotation) {
  CursorManager* cursor_manager = Shell::GetInstance()->cursor_manager();
  CursorManagerTestApi test_api(cursor_manager);

  gfx::Display display(0);
  display.set_device_scale_factor(2.0f);
  cursor_manager->SetDisplay(display);
  EXPECT_EQ(2.0f, test_api.GetDisplay().device_scale_factor());
  EXPECT_EQ(gfx::Display::ROTATE_0, test_api.GetDisplay().rotation());

  display.set_device_scale_factor(1.0f);
  display.set_rotation(gfx::Display::ROTATE_270);
  cursor_manager->SetDisplay(display);
  EXPECT_EQ(1.0f, test_api.GetDisplay().device_scale_factor());
  EXPECT_EQ(gfx::Display::ROTATE_270, test_api.GetDisplay().rotation());
}

#if defined(OS_WIN)
// Temporarily disabled for windows. See crbug.com/112222.
#define MAYBE_DisabledMouseEventsLocation DISABLED_DisabledMouseEventsLocation
#else
#define MAYBE_DisabledMouseEventsLocation DisabledMouseEventsLocation
#endif  // defined(OS_WIN)

// Verifies that RootWindow generates a mouse event located outside of a window
// when mouse events are disabled.
TEST_F(AshNativeCursorManagerTest, MAYBE_DisabledMouseEventsLocation) {
  scoped_ptr<MouseEventLocationDelegate> delegate(
      new MouseEventLocationDelegate());
  const int kWindowWidth = 123;
  const int kWindowHeight = 45;
  gfx::Rect bounds(100, 200, kWindowWidth, kWindowHeight);
  scoped_ptr<aura::Window> window(aura::test::CreateTestWindowWithDelegate(
      delegate.get(), 1, bounds, Shell::GetInstance()->GetPrimaryRootWindow()));

  CursorManager* cursor_manager = Shell::GetInstance()->cursor_manager();
  cursor_manager->EnableMouseEvents();
  // Send a mouse event to window.
  gfx::Point point(101, 201);
  gfx::Point local_point;
  ui::MouseEvent event(ui::ET_MOUSE_MOVED, point, point, 0);
  aura::RootWindow* root_window = window->GetRootWindow();
  root_window->AsRootWindowHostDelegate()->OnHostMouseEvent(&event);

  // Location was in window.
  local_point = delegate->GetMouseEventLocationAndReset();
  aura::Window::ConvertPointToTarget(window.get(), root_window, &local_point);
  EXPECT_TRUE(window->bounds().Contains(local_point));

  // Location is now out of window.
  cursor_manager->DisableMouseEvents();
  RunAllPendingInMessageLoop();
  local_point = delegate->GetMouseEventLocationAndReset();
  aura::Window::ConvertPointToTarget(window.get(), root_window, &local_point);
  EXPECT_FALSE(window->bounds().Contains(local_point));
  EXPECT_FALSE(window->bounds().Contains(
      gfx::Screen::GetScreenFor(window.get())->GetCursorScreenPoint()));

  // Location is back in window.
  cursor_manager->EnableMouseEvents();
  RunAllPendingInMessageLoop();
  local_point = delegate->GetMouseEventLocationAndReset();
  aura::Window::ConvertPointToTarget(window.get(), root_window, &local_point);
  EXPECT_TRUE(window->bounds().Contains(local_point));
}

#if defined(OS_WIN)
// Disable on Win because RootWindow::MoveCursorTo is not implemented.
#define MAYBE_DisabledQueryMouseLocation DISABLED_DisabledQueryMouseLocation
#else
#define MAYBE_DisabledQueryMouseLocation DisabledQueryMouseLocation
#endif  // defined(OS_WIN)

TEST_F(AshNativeCursorManagerTest, MAYBE_DisabledQueryMouseLocation) {
  aura::RootWindow* root_window = Shell::GetInstance()->GetPrimaryRootWindow();
  root_window->MoveCursorTo(gfx::Point(10, 10));
  gfx::Point mouse_location;
  EXPECT_TRUE(root_window->QueryMouseLocationForTest(&mouse_location));
  EXPECT_EQ("10,10", mouse_location.ToString());
  Shell::GetInstance()->cursor_manager()->DisableMouseEvents();
  EXPECT_FALSE(root_window->QueryMouseLocationForTest(&mouse_location));
  EXPECT_EQ("0,0", mouse_location.ToString());
}

}  // namespace test
}  // namespace ash
