// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/cursor_manager.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/cursor_manager_test_api.h"
#include "ash/wm/image_cursors.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"

namespace ash {
namespace test {

namespace {

// A delegate for recording a mouse event location.
class MouseEventLocationDelegate : public aura::test::TestWindowDelegate {
 public:
  MouseEventLocationDelegate() {}
  virtual ~MouseEventLocationDelegate() {}

  const gfx::Point& mouse_event_location() const {
    return mouse_event_location_;
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

typedef test::AshTestBase CursorManagerTest;

TEST_F(CursorManagerTest, LockCursor) {
  CursorManager* cursor_manager = Shell::GetInstance()->cursor_manager();
  CursorManagerTestApi test_api(cursor_manager);

  cursor_manager->SetCursor(ui::kCursorCopy);
  EXPECT_EQ(ui::kCursorCopy, test_api.GetCurrentCursor().native_type());
  cursor_manager->SetDeviceScaleFactor(2.0f);
  EXPECT_EQ(2.0f, test_api.GetDeviceScaleFactor());
  EXPECT_TRUE(test_api.GetCurrentCursor().platform());

  cursor_manager->LockCursor();
  EXPECT_TRUE(cursor_manager->is_cursor_locked());

  // Cursor type does not change while cursor is locked.
  cursor_manager->SetCursor(ui::kCursorPointer);
  EXPECT_EQ(ui::kCursorCopy, test_api.GetCurrentCursor().native_type());

  // Device scale factor does change even while cursor is locked.
  cursor_manager->SetDeviceScaleFactor(1.0f);
  EXPECT_EQ(1.0f, test_api.GetDeviceScaleFactor());

  cursor_manager->UnlockCursor();
  EXPECT_FALSE(cursor_manager->is_cursor_locked());

  // Cursor type changes to the one specified while cursor is locked.
  EXPECT_EQ(ui::kCursorPointer, test_api.GetCurrentCursor().native_type());
  EXPECT_EQ(1.0f, test_api.GetDeviceScaleFactor());
  EXPECT_TRUE(test_api.GetCurrentCursor().platform());
}

TEST_F(CursorManagerTest, SetCursor) {
  CursorManager* cursor_manager = Shell::GetInstance()->cursor_manager();
  CursorManagerTestApi test_api(cursor_manager);

  cursor_manager->SetCursor(ui::kCursorCopy);
  EXPECT_EQ(ui::kCursorCopy, test_api.GetCurrentCursor().native_type());
  EXPECT_TRUE(test_api.GetCurrentCursor().platform());
  cursor_manager->SetCursor(ui::kCursorPointer);
  EXPECT_EQ(ui::kCursorPointer, test_api.GetCurrentCursor().native_type());
  EXPECT_TRUE(test_api.GetCurrentCursor().platform());
}

TEST_F(CursorManagerTest, ShowHideCursor) {
  CursorManager* cursor_manager = Shell::GetInstance()->cursor_manager();
  CursorManagerTestApi test_api(cursor_manager);

  cursor_manager->SetCursor(ui::kCursorCopy);
  EXPECT_EQ(ui::kCursorCopy, test_api.GetCurrentCursor().native_type());

  cursor_manager->ShowCursor();
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  cursor_manager->HideCursor();
  EXPECT_FALSE(cursor_manager->IsCursorVisible());
  // The current cursor does not change even when the cursor is not shown.
  EXPECT_EQ(ui::kCursorCopy, test_api.GetCurrentCursor().native_type());

  // Check if cursor visibility is locked.
  cursor_manager->LockCursor();
  EXPECT_FALSE(cursor_manager->IsCursorVisible());
  cursor_manager->ShowCursor();
  EXPECT_FALSE(cursor_manager->IsCursorVisible());
  cursor_manager->UnlockCursor();
  EXPECT_TRUE(cursor_manager->IsCursorVisible());

  cursor_manager->LockCursor();
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  cursor_manager->HideCursor();
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  cursor_manager->UnlockCursor();
  EXPECT_FALSE(cursor_manager->IsCursorVisible());

  // Checks setting visiblity while cursor is locked does not affect the
  // subsequent uses of UnlockCursor.
  cursor_manager->LockCursor();
  cursor_manager->HideCursor();
  cursor_manager->UnlockCursor();
  EXPECT_FALSE(cursor_manager->IsCursorVisible());

  cursor_manager->ShowCursor();
  cursor_manager->LockCursor();
  cursor_manager->UnlockCursor();
  EXPECT_TRUE(cursor_manager->IsCursorVisible());

  cursor_manager->LockCursor();
  cursor_manager->ShowCursor();
  cursor_manager->UnlockCursor();
  EXPECT_TRUE(cursor_manager->IsCursorVisible());

  cursor_manager->HideCursor();
  cursor_manager->LockCursor();
  cursor_manager->UnlockCursor();
  EXPECT_FALSE(cursor_manager->IsCursorVisible());
}

TEST_F(CursorManagerTest, SetDeviceScaleFactor) {
  CursorManager* cursor_manager = Shell::GetInstance()->cursor_manager();
  CursorManagerTestApi test_api(cursor_manager);

  cursor_manager->SetDeviceScaleFactor(2.0f);
  EXPECT_EQ(2.0f, test_api.GetDeviceScaleFactor());
  cursor_manager->SetDeviceScaleFactor(1.0f);
  EXPECT_EQ(1.0f, test_api.GetDeviceScaleFactor());
}

// Verifies that LockCursor/UnlockCursor work correctly with
// EnableMouseEvents and DisableMouseEvents
TEST_F(CursorManagerTest, EnableDisableMouseEvents) {
  CursorManager* cursor_manager = Shell::GetInstance()->cursor_manager();
  CursorManagerTestApi test_api(cursor_manager);

  cursor_manager->SetCursor(ui::kCursorCopy);
  EXPECT_EQ(ui::kCursorCopy, test_api.GetCurrentCursor().native_type());

  cursor_manager->EnableMouseEvents();
  EXPECT_TRUE(cursor_manager->IsMouseEventsEnabled());
  cursor_manager->DisableMouseEvents();
  EXPECT_FALSE(cursor_manager->IsMouseEventsEnabled());
  // The current cursor does not change even when the cursor is not shown.
  EXPECT_EQ(ui::kCursorCopy, test_api.GetCurrentCursor().native_type());

  // Check if cursor enable state is locked.
  cursor_manager->LockCursor();
  EXPECT_FALSE(cursor_manager->IsMouseEventsEnabled());
  cursor_manager->EnableMouseEvents();
  EXPECT_FALSE(cursor_manager->IsMouseEventsEnabled());
  cursor_manager->UnlockCursor();
  EXPECT_TRUE(cursor_manager->IsMouseEventsEnabled());

  cursor_manager->LockCursor();
  EXPECT_TRUE(cursor_manager->IsMouseEventsEnabled());
  cursor_manager->DisableMouseEvents();
  EXPECT_TRUE(cursor_manager->IsMouseEventsEnabled());
  cursor_manager->UnlockCursor();
  EXPECT_FALSE(cursor_manager->IsMouseEventsEnabled());

  // Checks enabling cursor while cursor is locked does not affect the
  // subsequent uses of UnlockCursor.
  cursor_manager->LockCursor();
  cursor_manager->DisableMouseEvents();
  cursor_manager->UnlockCursor();
  EXPECT_FALSE(cursor_manager->IsMouseEventsEnabled());

  cursor_manager->EnableMouseEvents();
  cursor_manager->LockCursor();
  cursor_manager->UnlockCursor();
  EXPECT_TRUE(cursor_manager->IsMouseEventsEnabled());

  cursor_manager->LockCursor();
  cursor_manager->EnableMouseEvents();
  cursor_manager->UnlockCursor();
  EXPECT_TRUE(cursor_manager->IsMouseEventsEnabled());

  cursor_manager->DisableMouseEvents();
  cursor_manager->LockCursor();
  cursor_manager->UnlockCursor();
  EXPECT_FALSE(cursor_manager->IsMouseEventsEnabled());
}

TEST_F(CursorManagerTest, IsMouseEventsEnabled) {
  CursorManager* cursor_manager = Shell::GetInstance()->cursor_manager();
  cursor_manager->EnableMouseEvents();
  EXPECT_TRUE(cursor_manager->IsMouseEventsEnabled());
  cursor_manager->DisableMouseEvents();
  EXPECT_FALSE(cursor_manager->IsMouseEventsEnabled());
}

// Verifies that the mouse events enable state changes correctly when
// ShowCursor/HideCursor and EnableMouseEvents/DisableMouseEvents are used
// together.
TEST_F(CursorManagerTest, ShowAndEnable) {
  CursorManager* cursor_manager = Shell::GetInstance()->cursor_manager();

  // Changing the visibility of the cursor does not affect the enable state.
  cursor_manager->EnableMouseEvents();
  cursor_manager->ShowCursor();
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  EXPECT_TRUE(cursor_manager->IsMouseEventsEnabled());
  cursor_manager->HideCursor();
  EXPECT_FALSE(cursor_manager->IsCursorVisible());
  EXPECT_TRUE(cursor_manager->IsMouseEventsEnabled());
  cursor_manager->ShowCursor();
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  EXPECT_TRUE(cursor_manager->IsMouseEventsEnabled());

  // When mouse events are disabled, it also gets invisible.
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  cursor_manager->DisableMouseEvents();
  EXPECT_FALSE(cursor_manager->IsCursorVisible());
  EXPECT_FALSE(cursor_manager->IsMouseEventsEnabled());

  // When mouse events are enabled, it restores the visibility state.
  cursor_manager->EnableMouseEvents();
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  EXPECT_TRUE(cursor_manager->IsMouseEventsEnabled());

  cursor_manager->ShowCursor();
  cursor_manager->DisableMouseEvents();
  EXPECT_FALSE(cursor_manager->IsCursorVisible());
  EXPECT_FALSE(cursor_manager->IsMouseEventsEnabled());
  cursor_manager->EnableMouseEvents();
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  EXPECT_TRUE(cursor_manager->IsMouseEventsEnabled());

  cursor_manager->HideCursor();
  cursor_manager->DisableMouseEvents();
  EXPECT_FALSE(cursor_manager->IsCursorVisible());
  EXPECT_FALSE(cursor_manager->IsMouseEventsEnabled());
  cursor_manager->EnableMouseEvents();
  EXPECT_FALSE(cursor_manager->IsCursorVisible());
  EXPECT_TRUE(cursor_manager->IsMouseEventsEnabled());

  // When mouse events are disabled, ShowCursor is ignored.
  cursor_manager->DisableMouseEvents();
  EXPECT_FALSE(cursor_manager->IsCursorVisible());
  EXPECT_FALSE(cursor_manager->IsMouseEventsEnabled());
  cursor_manager->ShowCursor();
  EXPECT_FALSE(cursor_manager->IsCursorVisible());
  EXPECT_FALSE(cursor_manager->IsMouseEventsEnabled());
  cursor_manager->DisableMouseEvents();
  EXPECT_FALSE(cursor_manager->IsCursorVisible());
  EXPECT_FALSE(cursor_manager->IsMouseEventsEnabled());
}

// Verifies that calling DisableMouseEvents multiple times in a row makes no
// difference compared with calling it once.
// This is a regression test for http://crbug.com/169404.
TEST_F(CursorManagerTest, MultipleDisableMouseEvents) {
  CursorManager* cursor_manager = Shell::GetInstance()->cursor_manager();
  cursor_manager->DisableMouseEvents();
  cursor_manager->DisableMouseEvents();
  cursor_manager->EnableMouseEvents();
  cursor_manager->LockCursor();
  cursor_manager->UnlockCursor();
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
}

// Verifies that calling EnableMouseEvents multiple times in a row makes no
// difference compared with calling it once.
TEST_F(CursorManagerTest, MultipleEnableMouseEvents) {
  CursorManager* cursor_manager = Shell::GetInstance()->cursor_manager();
  cursor_manager->DisableMouseEvents();
  cursor_manager->EnableMouseEvents();
  cursor_manager->EnableMouseEvents();
  cursor_manager->LockCursor();
  cursor_manager->UnlockCursor();
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
}

#if defined(OS_WIN)
// Temporarily disabled for windows. See crbug.com/112222.
#define MAYBE_DisabledMouseEventsLocation DISABLED_DisabledMouseEventsLocation
#else
#define MAYBE_DisabledMouseEventsLocation DisabledMouseEventsLocation
#endif  // defined(OS_WIN)

// Verifies that RootWindow generates a mouse event located outside of a window
// when mouse events are disabled.
TEST_F(CursorManagerTest, MAYBE_DisabledMouseEventsLocation) {
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
  local_point = delegate->mouse_event_location();
  aura::Window::ConvertPointToTarget(window.get(), root_window, &local_point);
  EXPECT_TRUE(window->bounds().Contains(local_point));

  // Location is now out of window.
  cursor_manager->DisableMouseEvents();
  RunAllPendingInMessageLoop();
  local_point = delegate->mouse_event_location();
  aura::Window::ConvertPointToTarget(window.get(), root_window, &local_point);
  EXPECT_FALSE(window->bounds().Contains(local_point));

  // Location is back in window.
  cursor_manager->EnableMouseEvents();
  RunAllPendingInMessageLoop();
  local_point = delegate->mouse_event_location();
  aura::Window::ConvertPointToTarget(window.get(), root_window, &local_point);
  EXPECT_TRUE(window->bounds().Contains(local_point));
}

#if defined(OS_WIN)
// Disable on Win because RootWindow::MoveCursorTo is not implemented.
#define MAYBE_DisabledQueryMouseLocation DISABLED_DisabledQueryMouseLocation
#else
#define MAYBE_DisabledQueryMouseLocation DisabledQueryMouseLocation
#endif  // defined(OS_WIN)

TEST_F(CursorManagerTest, MAYBE_DisabledQueryMouseLocation) {
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
