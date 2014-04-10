// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/ash_native_cursor_manager.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/cursor_manager_test_api.h"
#include "ash/wm/image_cursors.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/gfx/screen.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "ui/base/cursor/cursor_loader_win.h"
#endif

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
  ::wm::CursorManager* cursor_manager = Shell::GetInstance()->cursor_manager();
  CursorManagerTestApi test_api(cursor_manager);
  gfx::Display display(0);
#if defined(OS_WIN)
  ui::CursorLoaderWin::SetCursorResourceModule(L"ash_unittests.exe");
#endif
  cursor_manager->SetCursor(ui::kCursorCopy);
  EXPECT_EQ(ui::kCursorCopy, test_api.GetCurrentCursor().native_type());
  display.set_device_scale_factor(2.0f);
  display.set_rotation(gfx::Display::ROTATE_90);
  cursor_manager->SetScale(2.5f);
  cursor_manager->SetDisplay(display);
  EXPECT_EQ(2.5f, test_api.GetCurrentScale());
  EXPECT_EQ(2.0f, test_api.GetDisplay().device_scale_factor());
  EXPECT_EQ(ui::CURSOR_SET_NORMAL, test_api.GetCurrentCursorSet());
  EXPECT_EQ(gfx::Display::ROTATE_90, test_api.GetDisplay().rotation());
  EXPECT_TRUE(test_api.GetCurrentCursor().platform());

  cursor_manager->LockCursor();
  EXPECT_TRUE(cursor_manager->IsCursorLocked());

  // Cursor type does not change while cursor is locked.
  EXPECT_EQ(ui::CURSOR_SET_NORMAL, test_api.GetCurrentCursorSet());
  cursor_manager->SetCursorSet(ui::CURSOR_SET_NORMAL);
  EXPECT_EQ(ui::CURSOR_SET_NORMAL, test_api.GetCurrentCursorSet());
  cursor_manager->SetCursorSet(ui::CURSOR_SET_LARGE);
  EXPECT_EQ(ui::CURSOR_SET_LARGE, test_api.GetCurrentCursorSet());
  cursor_manager->SetCursorSet(ui::CURSOR_SET_NORMAL);
  EXPECT_EQ(ui::CURSOR_SET_NORMAL, test_api.GetCurrentCursorSet());

  // Cusror scale does change even while cursor is locked.
  EXPECT_EQ(2.5f, test_api.GetCurrentScale());
  cursor_manager->SetScale(1.f);
  EXPECT_EQ(1.f, test_api.GetCurrentScale());
  cursor_manager->SetScale(1.5f);
  EXPECT_EQ(1.5f, test_api.GetCurrentScale());

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
  EXPECT_FALSE(cursor_manager->IsCursorLocked());

  // Cursor type changes to the one specified while cursor is locked.
  EXPECT_EQ(1.5f, test_api.GetCurrentScale());
  EXPECT_EQ(ui::kCursorPointer, test_api.GetCurrentCursor().native_type());
  EXPECT_EQ(1.0f, test_api.GetDisplay().device_scale_factor());
  EXPECT_TRUE(test_api.GetCurrentCursor().platform());
}

TEST_F(AshNativeCursorManagerTest, SetCursor) {
  ::wm::CursorManager* cursor_manager = Shell::GetInstance()->cursor_manager();
  CursorManagerTestApi test_api(cursor_manager);
#if defined(OS_WIN)
  ui::CursorLoaderWin::SetCursorResourceModule(L"ash_unittests.exe");
#endif
  cursor_manager->SetCursor(ui::kCursorCopy);
  EXPECT_EQ(ui::kCursorCopy, test_api.GetCurrentCursor().native_type());
  EXPECT_TRUE(test_api.GetCurrentCursor().platform());
  cursor_manager->SetCursor(ui::kCursorPointer);
  EXPECT_EQ(ui::kCursorPointer, test_api.GetCurrentCursor().native_type());
  EXPECT_TRUE(test_api.GetCurrentCursor().platform());
}

TEST_F(AshNativeCursorManagerTest, SetCursorSet) {
  ::wm::CursorManager* cursor_manager = Shell::GetInstance()->cursor_manager();
  CursorManagerTestApi test_api(cursor_manager);

  EXPECT_EQ(ui::CURSOR_SET_NORMAL, test_api.GetCurrentCursorSet());

  cursor_manager->SetCursorSet(ui::CURSOR_SET_NORMAL);
  EXPECT_EQ(ui::CURSOR_SET_NORMAL, test_api.GetCurrentCursorSet());

  cursor_manager->SetCursorSet(ui::CURSOR_SET_LARGE);
  EXPECT_EQ(ui::CURSOR_SET_LARGE, test_api.GetCurrentCursorSet());

  cursor_manager->SetCursorSet(ui::CURSOR_SET_NORMAL);
  EXPECT_EQ(ui::CURSOR_SET_NORMAL, test_api.GetCurrentCursorSet());
}

TEST_F(AshNativeCursorManagerTest, SetScale) {
  ::wm::CursorManager* cursor_manager = Shell::GetInstance()->cursor_manager();
  CursorManagerTestApi test_api(cursor_manager);

  EXPECT_EQ(1.f, test_api.GetCurrentScale());

  cursor_manager->SetScale(2.5f);
  EXPECT_EQ(2.5f, test_api.GetCurrentScale());

  cursor_manager->SetScale(1.f);
  EXPECT_EQ(1.f, test_api.GetCurrentScale());
}

TEST_F(AshNativeCursorManagerTest, SetDeviceScaleFactorAndRotation) {
  ::wm::CursorManager* cursor_manager = Shell::GetInstance()->cursor_manager();
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

TEST_F(AshNativeCursorManagerTest, DisabledQueryMouseLocation) {
  aura::Window* root_window = Shell::GetInstance()->GetPrimaryRootWindow();
#if defined(OS_WIN)
  if (base::win::GetVersion() < base::win::VERSION_WIN8)
    return;
  // On Windows 8 the ASH environment has two processes, the viewer process
  // which runs in Windows 8 mode and the browser process. The initialization
  // happens when the viewer process connects to the browser channel and sends
  // the initial IPC message.
  RunAllPendingInMessageLoop();
#endif
  root_window->MoveCursorTo(gfx::Point(10, 10));
#if defined(OS_WIN)
  // The MoveCursor operation on Windows 8 is implemented in the viewer process
  // which is notified by an IPC message to perform the MoveCursor operation.
  // We need to ensure that the IPC is delivered to the viewer process and it
  // the ACK is sent back from the viewer indicating that the operation
  // completed.
  Sleep(100);
  RunAllPendingInMessageLoop();
#endif
  aura::WindowTreeHost* host = root_window->GetHost();
  gfx::Point mouse_location;
  EXPECT_TRUE(host->QueryMouseLocation(&mouse_location));
  EXPECT_EQ("10,10", mouse_location.ToString());
  Shell::GetInstance()->cursor_manager()->DisableMouseEvents();
  EXPECT_FALSE(host->QueryMouseLocation(&mouse_location));
  EXPECT_EQ("0,0", mouse_location.ToString());
}

}  // namespace test
}  // namespace ash
