// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/mirror_window_controller.h"

#include "ash/display/mirror_window_test_api.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/public/cpp/config.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/cursor_manager_test_api.h"
#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "ui/aura/env.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/hit_test.h"
#include "ui/display/display_switches.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/test/event_generator.h"

namespace ash {

namespace {
display::ManagedDisplayInfo CreateDisplayInfo(int64_t id,
                                              const gfx::Rect& bounds) {
  display::ManagedDisplayInfo info(
      id, base::StringPrintf("x-%d", static_cast<int>(id)), false);
  info.SetBounds(bounds);
  return info;
}

class MirrorOnBootTest : public AshTestBase {
 public:
  MirrorOnBootTest() = default;
  ~MirrorOnBootTest() override = default;

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        ::switches::kHostWindowBounds, "1+1-300x300,1+301-300x300");
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ::switches::kEnableSoftwareMirroring);
    AshTestBase::SetUp();
  }
  void TearDown() override { AshTestBase::TearDown(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(MirrorOnBootTest);
};
}

using MirrorWindowControllerTest = AshTestBase;

class MirrorWindowControllerTestDisableMultiMirroring : public AshTestBase {
 public:
  MirrorWindowControllerTestDisableMultiMirroring() = default;
  ~MirrorWindowControllerTestDisableMultiMirroring() override = default;

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ::switches::kDisableMultiMirroring);
    AshTestBase::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MirrorWindowControllerTestDisableMultiMirroring);
};

// TODO(weidongg/774795) Remove this test when multi mirroring is enabled by
// default, because cursor compositing will be enabled for software mirroring.
TEST_F(MirrorWindowControllerTestDisableMultiMirroring, MirrorCursorBasic) {
  // MirrorWindowController is not used in the MUS or MASH configs.
  if (Shell::GetAshConfig() != Config::CLASSIC)
    return;

  MirrorWindowTestApi test_api;
  aura::test::TestWindowDelegate test_window_delegate;
  test_window_delegate.set_window_component(HTTOP);

  UpdateDisplay("400x400,400x400");
  display_manager()->SetMirrorMode(display::MirrorMode::kNormal, base::nullopt);
  RunAllPendingInMessageLoop();
  aura::Window* root = Shell::Get()->GetPrimaryRootWindow();
  std::unique_ptr<aura::Window> window(aura::test::CreateTestWindowWithDelegate(
      &test_window_delegate, 0, gfx::Rect(50, 50, 100, 100), root));
  window->Show();
  window->SetName("foo");

  EXPECT_TRUE(test_api.GetCursorWindow());
  EXPECT_EQ("50,50 100x100", window->bounds().ToString());

  ui::test::EventGenerator generator(root);
  generator.MoveMouseTo(10, 10);

  // Test if cursor movement is propertly reflected in mirror window.
  EXPECT_EQ("4,4", test_api.GetCursorHotPoint().ToString());
  EXPECT_EQ("10,10",
            test_api.GetCursorHotPointLocationInRootWindow().ToString());
  EXPECT_EQ(ui::CursorType::kNull, test_api.GetCurrentCursorType());
  EXPECT_TRUE(test_api.GetCursorWindow()->IsVisible());

  // Test if cursor type change is propertly reflected in mirror window.
  generator.MoveMouseTo(100, 100);
  EXPECT_EQ("100,100",
            test_api.GetCursorHotPointLocationInRootWindow().ToString());
  EXPECT_EQ(ui::CursorType::kNorthResize, test_api.GetCurrentCursorType());

  // Test if visibility change is propertly reflected in mirror window.
  // A key event hides cursor.
  generator.PressKey(ui::VKEY_A, 0);
  generator.ReleaseKey(ui::VKEY_A, 0);
  EXPECT_FALSE(test_api.GetCursorWindow()->IsVisible());

  // Mouse event makes it visible again.
  generator.MoveMouseTo(300, 300);
  EXPECT_EQ("300,300",
            test_api.GetCursorHotPointLocationInRootWindow().ToString());
  EXPECT_EQ(ui::CursorType::kNull, test_api.GetCurrentCursorType());
  EXPECT_TRUE(test_api.GetCursorWindow()->IsVisible());
}

// TODO(weidongg/774795) Remove this test when multi mirroring is enabled by
// default, because cursor compositing will be enabled for software mirroring.
TEST_F(MirrorWindowControllerTestDisableMultiMirroring, MirrorCursorRotate) {
  // MirrorWindowController is not used in the MUS or MASH configs.
  if (Shell::GetAshConfig() != Config::CLASSIC)
    return;

  MirrorWindowTestApi test_api;
  aura::test::TestWindowDelegate test_window_delegate;
  test_window_delegate.set_window_component(HTTOP);

  UpdateDisplay("400x400,400x400");
  display_manager()->SetMirrorMode(display::MirrorMode::kNormal, base::nullopt);
  RunAllPendingInMessageLoop();
  aura::Window* root = Shell::Get()->GetPrimaryRootWindow();
  std::unique_ptr<aura::Window> window(aura::test::CreateTestWindowWithDelegate(
      &test_window_delegate, 0, gfx::Rect(50, 50, 100, 100), root));
  window->Show();
  window->SetName("foo");

  EXPECT_TRUE(test_api.GetCursorWindow());
  EXPECT_EQ("50,50 100x100", window->bounds().ToString());

  ui::test::EventGenerator generator(root);
  generator.MoveMouseToInHost(100, 100);

  // Test if cursor movement is propertly reflected in mirror window.
  EXPECT_EQ("11,12", test_api.GetCursorHotPoint().ToString());
  EXPECT_EQ("100,100",
            test_api.GetCursorHotPointLocationInRootWindow().ToString());
  EXPECT_EQ(ui::CursorType::kNorthResize, test_api.GetCurrentCursorType());

  UpdateDisplay("400x400/r,400x400");  // 90 degrees.
  generator.MoveMouseToInHost(300, 100);
  EXPECT_EQ(ui::CursorType::kNorthResize, test_api.GetCurrentCursorType());
  // The size of cursor image is 25x25, so the rotated hot point must
  // be (25-12, 11).
  EXPECT_EQ("13,11", test_api.GetCursorHotPoint().ToString());
  EXPECT_EQ("300,100",
            test_api.GetCursorHotPointLocationInRootWindow().ToString());

  UpdateDisplay("400x400/u,400x400");  // 180 degrees.
  generator.MoveMouseToInHost(300, 300);
  EXPECT_EQ(ui::CursorType::kNorthResize, test_api.GetCurrentCursorType());
  // Rotated hot point must be (25-11, 25-12).
  EXPECT_EQ("14,13", test_api.GetCursorHotPoint().ToString());
  EXPECT_EQ("300,300",
            test_api.GetCursorHotPointLocationInRootWindow().ToString());

  UpdateDisplay("400x400/l,400x400");  // 270 degrees.
  generator.MoveMouseToInHost(100, 300);
  EXPECT_EQ(ui::CursorType::kNorthResize, test_api.GetCurrentCursorType());
  // Rotated hot point must be (12, 25-11).
  EXPECT_EQ("12,14", test_api.GetCursorHotPoint().ToString());
  EXPECT_EQ("100,300",
            test_api.GetCursorHotPointLocationInRootWindow().ToString());
}

// Make sure that the mirror cursor's location is same as
// the source display's host location in the mirror root window's
// coordinates.
// TODO(weidongg/774795) Remove this test when multi mirroring is enabled by
// default, because cursor compositing will be enabled for software mirroring.
TEST_F(MirrorWindowControllerTestDisableMultiMirroring, MirrorCursorLocations) {
  // MirrorWindowController is not used in the MUS or MASH configs.
  if (Shell::GetAshConfig() != Config::CLASSIC)
    return;

  MirrorWindowTestApi test_api;

  // Test with device scale factor.
  UpdateDisplay("400x600*2,400x600");
  display_manager()->SetMirrorMode(display::MirrorMode::kNormal, base::nullopt);
  RunAllPendingInMessageLoop();

  aura::Window* root = Shell::Get()->GetPrimaryRootWindow();
  ui::test::EventGenerator generator(root);
  generator.MoveMouseToInHost(10, 20);

  EXPECT_EQ("7,7", test_api.GetCursorHotPoint().ToString());
  EXPECT_EQ("10,20",
            test_api.GetCursorHotPointLocationInRootWindow().ToString());

  // Test with ui scale
  UpdateDisplay("400x600@0.5,400x600");
  generator.MoveMouseToInHost(20, 30);

  EXPECT_EQ("4,4", test_api.GetCursorHotPoint().ToString());
  EXPECT_EQ("20,30",
            test_api.GetCursorHotPointLocationInRootWindow().ToString());

  // Test with rotation
  UpdateDisplay("400x600/r,400x600");
  generator.MoveMouseToInHost(30, 40);

  EXPECT_EQ("21,4", test_api.GetCursorHotPoint().ToString());
  EXPECT_EQ("30,40",
            test_api.GetCursorHotPointLocationInRootWindow().ToString());
}

// Test the behavior of the cursor when entering software mirror mode swaps the
// cursor's display.
// TODO(weidongg/774795) Remove this test when multi mirroring is enabled by
// default, because cursor compositing will be enabled for software mirroring.
TEST_F(MirrorWindowControllerTestDisableMultiMirroring,
       MirrorCursorMoveOnEnter) {
  // MirrorWindowController is not used in the MUS or MASH configs.
  if (Shell::GetAshConfig() != Config::CLASSIC)
    return;

  aura::Env* env = aura::Env::GetInstance();
  Shell* shell = Shell::Get();
  WindowTreeHostManager* window_tree_host_manager =
      shell->window_tree_host_manager();

  UpdateDisplay("400x400*2/r,400x400");
  int64_t primary_display_id = window_tree_host_manager->GetPrimaryDisplayId();
  int64_t secondary_display_id = display_manager()->GetSecondaryDisplay().id();
  display::test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                         primary_display_id);

  // Chrome uses the internal display as the source display for software mirror
  // mode. Move the cursor to the external display.
  aura::Window* secondary_root_window =
      window_tree_host_manager->GetRootWindowForDisplayId(secondary_display_id);
  secondary_root_window->MoveCursorTo(gfx::Point(100, 200));
  EXPECT_EQ("300,200", env->last_mouse_location().ToString());
  CursorManagerTestApi cursor_test_api(shell->cursor_manager());
  EXPECT_EQ(1.0f, cursor_test_api.GetCurrentCursor().device_scale_factor());
  EXPECT_EQ(display::Display::ROTATE_0,
            cursor_test_api.GetCurrentCursorRotation());

  UpdateDisplay("400x400*2/r,400x400");
  display_manager()->SetMirrorMode(display::MirrorMode::kNormal, base::nullopt);
  RunAllPendingInMessageLoop();

  // Entering mirror mode should have centered the cursor on the primary display
  // because the cursor's previous position is out of bounds.
  // Check real cursor's position and properties.
  EXPECT_EQ("100,100", env->last_mouse_location().ToString());
  EXPECT_EQ(2.0f, cursor_test_api.GetCurrentCursor().device_scale_factor());
  EXPECT_EQ(display::Display::ROTATE_90,
            cursor_test_api.GetCurrentCursorRotation());

  // Check mirrored cursor's location.
  MirrorWindowTestApi test_api;
  // The hot point location depends on the specific cursor.
  EXPECT_EQ(ui::CursorType::kNull, test_api.GetCurrentCursorType());
  // Rotated hot point must be (25-7, 7).
  EXPECT_EQ("18,7", test_api.GetCursorHotPoint().ToString());
  // New coordinates are not (200,200) because (200,200) is not the center of
  // the display.
  EXPECT_EQ("200,200",
            test_api.GetCursorHotPointLocationInRootWindow().ToString());
}

// Make sure that the compositor based mirroring can switch
// from/to dock mode.
TEST_F(MirrorWindowControllerTest, DockMode) {
  const int64_t internal_id = 1;
  const int64_t external_id = 2;

  const display::ManagedDisplayInfo internal_display_info =
      CreateDisplayInfo(internal_id, gfx::Rect(0, 0, 500, 500));
  const display::ManagedDisplayInfo external_display_info =
      CreateDisplayInfo(external_id, gfx::Rect(1, 1, 100, 100));
  std::vector<display::ManagedDisplayInfo> display_info_list;

  // software mirroring.
  display_info_list.push_back(internal_display_info);
  display_info_list.push_back(external_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  const int64_t internal_display_id =
      display::test::DisplayManagerTestApi(display_manager())
          .SetFirstDisplayAsInternalDisplay();
  EXPECT_EQ(internal_id, internal_display_id);

  display_manager()->SetMirrorMode(display::MirrorMode::kNormal, base::nullopt);
  RunAllPendingInMessageLoop();
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  EXPECT_TRUE(display_manager()->IsInSoftwareMirrorMode());
  EXPECT_EQ(external_id,
            display_manager()->GetMirroringDestinationDisplayIdList()[0]);

  // dock mode.
  display_info_list.clear();
  display_info_list.push_back(external_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  EXPECT_FALSE(display_manager()->IsInMirrorMode());

  // back to software mirroring.
  display_info_list.clear();
  display_info_list.push_back(internal_display_info);
  display_info_list.push_back(external_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  EXPECT_TRUE(display_manager()->IsInMirrorMode());
  EXPECT_EQ(external_id,
            display_manager()->GetMirroringDestinationDisplayIdList()[0]);
}

TEST_F(MirrorOnBootTest, MirrorOnBoot) {
  EXPECT_TRUE(display_manager()->IsInMirrorMode());

  // MirrorWindowController is not used in the MUS or MASH configs.
  if (Shell::GetAshConfig() != Config::CLASSIC)
    return;

  RunAllPendingInMessageLoop();
  MirrorWindowTestApi test_api;
  EXPECT_EQ(1U, test_api.GetHosts().size());
}

}  // namespace ash
