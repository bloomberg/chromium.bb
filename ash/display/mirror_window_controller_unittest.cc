// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/mirror_window_controller.h"

#include "ash/display/display_manager.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/mirror_window_test_api.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"

namespace ash {
namespace internal {

typedef test::AshTestBase MirrorWindowControllerTest;

#if defined(OS_WIN)
// Software mirroring does not work on win.
#define MAYBE_MirrorCursorBasic DISABLED_MirrorCursorBasic
#define MAYBE_MirrorCursorLocations DISABLED_MirrorCursorLocations
#else
#define MAYBE_MirrorCursorBasic MirrorCursorBasic
#define MAYBE_MirrorCursorLocations MirrorCursorLocations
#endif

TEST_F(MirrorWindowControllerTest, MAYBE_MirrorCursorBasic) {
  test::MirrorWindowTestApi test_api;
  aura::test::TestWindowDelegate test_window_delegate;
  test_window_delegate.set_window_component(HTTOP);

  DisplayManager* display_manager = Shell::GetInstance()->display_manager();
  display_manager->SetSoftwareMirroring(true);
  UpdateDisplay("400x400,400x400");
  aura::RootWindow* root = Shell::GetInstance()->GetPrimaryRootWindow();
  scoped_ptr<aura::Window> window(aura::test::CreateTestWindowWithDelegate(
      &test_window_delegate,
      0,
      gfx::Rect(50, 50, 100, 100),
      root));
  window->Show();
  window->SetName("foo");

  EXPECT_TRUE(test_api.GetCursorWindow());
  EXPECT_EQ("50,50 100x100", window->bounds().ToString());

  aura::test::EventGenerator generator(root);
  generator.MoveMouseTo(10, 10);

  // Test if cursor movement is propertly reflected in mirror window.
  gfx::Point hot_point = test_api.GetCursorHotPoint();
  gfx::Point cursor_window_origin =
      test_api.GetCursorWindow()->bounds().origin();
  EXPECT_EQ("4,4", hot_point.ToString());
  EXPECT_EQ(10 - hot_point.x(), cursor_window_origin.x());
  EXPECT_EQ(10 - hot_point.y(), cursor_window_origin.y());
  EXPECT_EQ(ui::kCursorNull, test_api.GetCurrentCursorType());
  EXPECT_TRUE(test_api.GetCursorWindow()->IsVisible());

  // Test if cursor type change is propertly reflected in mirror window.
  generator.MoveMouseTo(100, 100);
  hot_point = test_api.GetCursorHotPoint();
  cursor_window_origin = test_api.GetCursorWindow()->bounds().origin();
  EXPECT_EQ(100 - hot_point.x(), cursor_window_origin.x());
  EXPECT_EQ(100 - hot_point.y(), cursor_window_origin.y());
  EXPECT_EQ(ui::kCursorNorthResize, test_api.GetCurrentCursorType());

  // Test if visibility change is propertly reflected in mirror window.
  // A key event hides cursor.
  generator.PressKey(ui::VKEY_A, 0);
  generator.ReleaseKey(ui::VKEY_A, 0);
  EXPECT_FALSE(test_api.GetCursorWindow()->IsVisible());

  // Mouse event makes it visible again.
  generator.MoveMouseTo(300, 300);
  hot_point = test_api.GetCursorHotPoint();
  cursor_window_origin = test_api.GetCursorWindow()->bounds().origin();
  EXPECT_EQ(300 - hot_point.x(), cursor_window_origin.x());
  EXPECT_EQ(300 - hot_point.y(), cursor_window_origin.y());
  EXPECT_EQ(ui::kCursorNull, test_api.GetCurrentCursorType());
  EXPECT_TRUE(test_api.GetCursorWindow()->IsVisible());
}

// Make sure that the mirror cursor's location is same as
// the source display's host location in the mirror root window's
// coordinates.
TEST_F(MirrorWindowControllerTest, MAYBE_MirrorCursorLocations) {
  test::MirrorWindowTestApi test_api;
  DisplayManager* display_manager = Shell::GetInstance()->display_manager();
  display_manager->SetSoftwareMirroring(true);

  // Test with device scale factor.
  UpdateDisplay("400x600*2,400x600");

  aura::RootWindow* root = Shell::GetInstance()->GetPrimaryRootWindow();
  aura::test::EventGenerator generator(root);
  generator.MoveMouseToInHost(10, 20);

  gfx::Point hot_point = test_api.GetCursorHotPoint();
  EXPECT_EQ("8,9", hot_point.ToString());
  gfx::Point cursor_window_origin =
      test_api.GetCursorWindow()->bounds().origin();
  EXPECT_EQ(10 - hot_point.x(), cursor_window_origin.x());
  EXPECT_EQ(20 - hot_point.y(), cursor_window_origin.y());

  // Test with ui scale
  UpdateDisplay("400x600*0.5,400x600");
  generator.MoveMouseToInHost(20, 30);

  hot_point = test_api.GetCursorHotPoint();
  EXPECT_EQ("4,4", hot_point.ToString());
  cursor_window_origin = test_api.GetCursorWindow()->bounds().origin();
  EXPECT_EQ(20 - hot_point.x(), cursor_window_origin.x());
  EXPECT_EQ(30 - hot_point.y(), cursor_window_origin.y());

  // Test with rotation
  UpdateDisplay("400x600/r,400x600");
  generator.MoveMouseToInHost(30, 40);

  hot_point = test_api.GetCursorHotPoint();
  EXPECT_EQ("4,4", hot_point.ToString());
  cursor_window_origin = test_api.GetCursorWindow()->bounds().origin();
  EXPECT_EQ(30 - hot_point.x(), cursor_window_origin.x());
  EXPECT_EQ(40 - hot_point.y(), cursor_window_origin.y());
}

}  // namsspace internal
}  // namespace ash
