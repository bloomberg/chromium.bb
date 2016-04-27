// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/ash_native_cursor_manager.h"

#include "ash/display/display_info.h"
#include "ash/display/display_manager.h"
#include "ash/display/display_util.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/cursor_manager_test_api.h"
#include "ui/aura/test/aura_test_utils.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/cursor/image_cursors.h"
#include "ui/display/screen.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "ui/base/cursor/cursor_loader_win.h"
#endif

#if defined(USE_X11)
#include "ui/base/cursor/cursor_loader_x11.h"
#include "ui/resources/grit/ui_resources.h"
#endif

namespace ash {
namespace test {

namespace {

// A delegate for recording a mouse event location.
class MouseEventLocationDelegate : public aura::test::TestWindowDelegate {
 public:
  MouseEventLocationDelegate() {}
  ~MouseEventLocationDelegate() override {}

  gfx::Point GetMouseEventLocationAndReset() {
    gfx::Point p = mouse_event_location_;
    mouse_event_location_.SetPoint(-100, -100);
    return p;
  }

  void OnMouseEvent(ui::MouseEvent* event) override {
    mouse_event_location_ = event->location();
    event->SetHandled();
  }

 private:
  gfx::Point mouse_event_location_;

  DISALLOW_COPY_AND_ASSIGN(MouseEventLocationDelegate);
};

}  // namespace

typedef test::AshTestBase AshNativeCursorManagerTest;

#if defined(OS_WIN) && !defined(USE_ASH)
// TODO(msw): Times out on Windows. http://crbug.com/584038
#define MAYBE_LockCursor DISABLED_LockCursor
#else
#define MAYBE_LockCursor LockCursor
#endif
TEST_F(AshNativeCursorManagerTest, MAYBE_LockCursor) {
  ::wm::CursorManager* cursor_manager = Shell::GetInstance()->cursor_manager();
  CursorManagerTestApi test_api(cursor_manager);

#if defined(OS_WIN)
  ui::CursorLoaderWin::SetCursorResourceModule(L"ash_unittests.exe");
#endif
  cursor_manager->SetCursor(ui::kCursorCopy);
  EXPECT_EQ(ui::kCursorCopy, test_api.GetCurrentCursor().native_type());
  UpdateDisplay("800x800*2/r");
  EXPECT_EQ(2.0f, test_api.GetCurrentCursor().device_scale_factor());
  EXPECT_EQ(ui::CURSOR_SET_NORMAL, test_api.GetCurrentCursorSet());
  EXPECT_EQ(display::Display::ROTATE_90, test_api.GetCurrentCursorRotation());
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

  // Cursor type does not change while cursor is locked.
  cursor_manager->SetCursor(ui::kCursorPointer);
  EXPECT_EQ(ui::kCursorCopy, test_api.GetCurrentCursor().native_type());

  // Device scale factor and rotation do change even while cursor is locked.
  UpdateDisplay("800x800/u");
  EXPECT_EQ(1.0f, test_api.GetCurrentCursor().device_scale_factor());
  EXPECT_EQ(display::Display::ROTATE_180, test_api.GetCurrentCursorRotation());

  cursor_manager->UnlockCursor();
  EXPECT_FALSE(cursor_manager->IsCursorLocked());

  // Cursor type changes to the one specified while cursor is locked.
  EXPECT_EQ(ui::kCursorPointer, test_api.GetCurrentCursor().native_type());
  EXPECT_EQ(1.0f, test_api.GetCurrentCursor().device_scale_factor());
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

#if defined(OS_WIN) && !defined(USE_ASH)
// TODO(msw): Times out on Windows. http://crbug.com/584038
#define MAYBE_SetDeviceScaleFactorAndRotation \
  DISABLED_SetDeviceScaleFactorAndRotation
#else
#define MAYBE_SetDeviceScaleFactorAndRotation SetDeviceScaleFactorAndRotation
#endif
TEST_F(AshNativeCursorManagerTest, MAYBE_SetDeviceScaleFactorAndRotation) {
  ::wm::CursorManager* cursor_manager = Shell::GetInstance()->cursor_manager();
  CursorManagerTestApi test_api(cursor_manager);
  UpdateDisplay("800x100*2");
  EXPECT_EQ(2.0f, test_api.GetCurrentCursor().device_scale_factor());
  EXPECT_EQ(display::Display::ROTATE_0, test_api.GetCurrentCursorRotation());

  UpdateDisplay("800x100/l");
  EXPECT_EQ(1.0f, test_api.GetCurrentCursor().device_scale_factor());
  EXPECT_EQ(display::Display::ROTATE_270, test_api.GetCurrentCursorRotation());
}

#if defined(OS_CHROMEOS)
// TODO(oshima): crbug.com/143619
TEST_F(AshNativeCursorManagerTest, FractionalScale) {
  ::wm::CursorManager* cursor_manager = Shell::GetInstance()->cursor_manager();
  CursorManagerTestApi test_api(cursor_manager);
  // Cursor should use the resource scale factor.
  UpdateDisplay("800x100*1.25");
  EXPECT_EQ(1.0f, test_api.GetCurrentCursor().device_scale_factor());
}
#endif

#if defined(OS_WIN) && !defined(USE_ASH)
// TODO(msw): Times out on Windows. http://crbug.com/584038
#define MAYBE_UIScaleShouldNotChangeCursor DISABLED_UIScaleShouldNotChangeCursor
#else
#define MAYBE_UIScaleShouldNotChangeCursor UIScaleShouldNotChangeCursor
#endif
TEST_F(AshNativeCursorManagerTest, MAYBE_UIScaleShouldNotChangeCursor) {
  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::Display::SetInternalDisplayId(display_id);

  ::wm::CursorManager* cursor_manager = Shell::GetInstance()->cursor_manager();
  CursorManagerTestApi test_api(cursor_manager);

  SetDisplayUIScale(display_id, 0.5f);
  EXPECT_EQ(
      1.0f,
      display::Screen::GetScreen()->GetPrimaryDisplay().device_scale_factor());
  EXPECT_EQ(1.0f, test_api.GetCurrentCursor().device_scale_factor());

  SetDisplayUIScale(display_id, 1.0f);

  // 2x display should keep using 2x cursor regardless of the UI scale.
  UpdateDisplay("800x800*2");
  EXPECT_EQ(
      2.0f,
      display::Screen::GetScreen()->GetPrimaryDisplay().device_scale_factor());
  EXPECT_EQ(2.0f, test_api.GetCurrentCursor().device_scale_factor());
  SetDisplayUIScale(display_id, 2.0f);
  EXPECT_EQ(
      1.0f,
      display::Screen::GetScreen()->GetPrimaryDisplay().device_scale_factor());
  EXPECT_EQ(2.0f, test_api.GetCurrentCursor().device_scale_factor());
}

#if defined(USE_X11)
// This test is in ash_unittests because ui_base_unittests does not include
// 2x assets. crbug.com/372541.
TEST_F(AshNativeCursorManagerTest, CursorLoaderX11Test) {
  const int kCursorId = 1;
  ui::CursorLoaderX11 loader;
  loader.set_scale(1.0f);

  loader.LoadImageCursor(kCursorId, IDR_AURA_CURSOR_MOVE, gfx::Point());
  const XcursorImage* image = loader.GetXcursorImageForTest(kCursorId);
  int height = image->height;
  int width = image->width;
  loader.UnloadAll();

  // Load 2x cursor and make sure its size is 2x of the 1x cusor.
  loader.set_scale(2.0f);
  loader.LoadImageCursor(kCursorId, IDR_AURA_CURSOR_MOVE, gfx::Point());
  image = loader.GetXcursorImageForTest(kCursorId);
  EXPECT_EQ(height * 2, static_cast<int>(image->height));
  EXPECT_EQ(width * 2, static_cast<int>(image->width));
}
#endif

}  // namespace test
}  // namespace ash
