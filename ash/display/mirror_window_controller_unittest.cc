// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/mirror_window_controller.h"

#include "ash/ash_switches.h"
#include "ash/display/display_manager.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/display_manager_test_api.h"
#include "ash/test/mirror_window_test_api.h"
#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/hit_test.h"
#include "ui/events/test/event_generator.h"

namespace ash {

namespace {
DisplayInfo CreateDisplayInfo(int64 id, const gfx::Rect& bounds) {
  DisplayInfo info(id, base::StringPrintf("x-%d", static_cast<int>(id)), false);
  info.SetBounds(bounds);
  return info;
}

class MirrorOnBootTest : public test::AshTestBase {
 public:
  MirrorOnBootTest() {}
  virtual ~MirrorOnBootTest() {}

  virtual void SetUp() OVERRIDE {
    CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kAshHostWindowBounds, "1+1-300x300,1+301-300x300");
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kAshEnableSoftwareMirroring);
    test::AshTestBase::SetUp();
  }
  virtual void TearDown() OVERRIDE {
    test::AshTestBase::TearDown();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MirrorOnBootTest);
};

}

typedef test::AshTestBase MirrorWindowControllerTest;

#if defined(OS_WIN)
// Software mirroring does not work on win.
#define MAYBE_MirrorCursorBasic DISABLED_MirrorCursorBasic
#define MAYBE_MirrorCursorLocations DISABLED_MirrorCursorLocations
#define MAYBE_MirrorCursorRotate DISABLED_MirrorCursorRotate
#define MAYBE_DockMode DISABLED_DockMode
#define MAYBE_MirrorOnBoot DISABLED_MirrorOnBoot
#else
#define MAYBE_MirrorCursorBasic MirrorCursorBasic
#define MAYBE_MirrorCursorLocations MirrorCursorLocations
#define MAYBE_MirrorCursorRotate MirrorCursorRotate
#define MAYBE_DockMode DockMode
#define MAYBE_MirrorOnBoot MirrorOnBoot
#endif

TEST_F(MirrorWindowControllerTest, MAYBE_MirrorCursorBasic) {
  test::MirrorWindowTestApi test_api;
  aura::test::TestWindowDelegate test_window_delegate;
  test_window_delegate.set_window_component(HTTOP);

  DisplayManager* display_manager = Shell::GetInstance()->display_manager();
  display_manager->SetSecondDisplayMode(DisplayManager::MIRRORING);
  UpdateDisplay("400x400,400x400");
  aura::Window* root = Shell::GetInstance()->GetPrimaryRootWindow();
  scoped_ptr<aura::Window> window(aura::test::CreateTestWindowWithDelegate(
      &test_window_delegate,
      0,
      gfx::Rect(50, 50, 100, 100),
      root));
  window->Show();
  window->SetName("foo");

  EXPECT_TRUE(test_api.GetCursorWindow());
  EXPECT_EQ("50,50 100x100", window->bounds().ToString());

  ui::test::EventGenerator generator(root);
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

TEST_F(MirrorWindowControllerTest, MAYBE_MirrorCursorRotate) {
  test::MirrorWindowTestApi test_api;
  aura::test::TestWindowDelegate test_window_delegate;
  test_window_delegate.set_window_component(HTTOP);

  DisplayManager* display_manager = Shell::GetInstance()->display_manager();
  display_manager->SetSecondDisplayMode(DisplayManager::MIRRORING);
  UpdateDisplay("400x400,400x400");
  aura::Window* root = Shell::GetInstance()->GetPrimaryRootWindow();
  scoped_ptr<aura::Window> window(aura::test::CreateTestWindowWithDelegate(
      &test_window_delegate,
      0,
      gfx::Rect(50, 50, 100, 100),
      root));
  window->Show();
  window->SetName("foo");

  EXPECT_TRUE(test_api.GetCursorWindow());
  EXPECT_EQ("50,50 100x100", window->bounds().ToString());

  ui::test::EventGenerator generator(root);
  generator.MoveMouseToInHost(100, 100);

  // Test if cursor movement is propertly reflected in mirror window.
  gfx::Point hot_point = test_api.GetCursorHotPoint();
  gfx::Point cursor_window_origin =
      test_api.GetCursorWindow()->bounds().origin();
  EXPECT_EQ("11,12", hot_point.ToString());
  EXPECT_EQ(100 - hot_point.x(), cursor_window_origin.x());
  EXPECT_EQ(100 - hot_point.y(), cursor_window_origin.y());
  EXPECT_EQ(ui::kCursorNorthResize, test_api.GetCurrentCursorType());

  UpdateDisplay("400x400/r,400x400");  // 90 degrees.
  generator.MoveMouseToInHost(300, 100);
  hot_point = test_api.GetCursorHotPoint();
  cursor_window_origin = test_api.GetCursorWindow()->bounds().origin();
  EXPECT_EQ(ui::kCursorNorthResize, test_api.GetCurrentCursorType());
  // The size of cursor image is 25x25, so the rotated hot point must
  // be (25-12, 11).
  EXPECT_EQ("13,11", hot_point.ToString());
  EXPECT_EQ(300 - hot_point.x(), cursor_window_origin.x());
  EXPECT_EQ(100 - hot_point.y(), cursor_window_origin.y());

  UpdateDisplay("400x400/u,400x400");  // 180 degrees.
  generator.MoveMouseToInHost(300, 300);
  hot_point = test_api.GetCursorHotPoint();
  cursor_window_origin = test_api.GetCursorWindow()->bounds().origin();
  EXPECT_EQ(ui::kCursorNorthResize, test_api.GetCurrentCursorType());
  // Rotated hot point must be (25-11, 25-12).
  EXPECT_EQ("14,13", hot_point.ToString());
  EXPECT_EQ(300 - hot_point.x(), cursor_window_origin.x());
  EXPECT_EQ(300 - hot_point.y(), cursor_window_origin.y());

  UpdateDisplay("400x400/l,400x400");  // 270 degrees.
  generator.MoveMouseToInHost(100, 300);
  hot_point = test_api.GetCursorHotPoint();
  cursor_window_origin = test_api.GetCursorWindow()->bounds().origin();
  EXPECT_EQ(ui::kCursorNorthResize, test_api.GetCurrentCursorType());
  // Rotated hot point must be (12, 25-11).
  EXPECT_EQ("12,14", hot_point.ToString());
  EXPECT_EQ(100 - hot_point.x(), cursor_window_origin.x());
  EXPECT_EQ(300 - hot_point.y(), cursor_window_origin.y());
}

// Make sure that the mirror cursor's location is same as
// the source display's host location in the mirror root window's
// coordinates.
TEST_F(MirrorWindowControllerTest, MAYBE_MirrorCursorLocations) {
  test::MirrorWindowTestApi test_api;
  DisplayManager* display_manager = Shell::GetInstance()->display_manager();
  display_manager->SetSecondDisplayMode(DisplayManager::MIRRORING);

  // Test with device scale factor.
  UpdateDisplay("400x600*2,400x600");

  aura::Window* root = Shell::GetInstance()->GetPrimaryRootWindow();
  ui::test::EventGenerator generator(root);
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
  EXPECT_EQ("21,4", hot_point.ToString());
  cursor_window_origin = test_api.GetCursorWindow()->bounds().origin();
  EXPECT_EQ(30 - hot_point.x(), cursor_window_origin.x());
  EXPECT_EQ(40 - hot_point.y(), cursor_window_origin.y());
}

// Make sure that the compositor based mirroring can switch
// from/to dock mode.
TEST_F(MirrorWindowControllerTest, MAYBE_DockMode) {
  DisplayManager* display_manager = Shell::GetInstance()->display_manager();
  const int64 internal_id = 1;
  const int64 external_id = 2;

  const DisplayInfo internal_display_info =
      CreateDisplayInfo(internal_id, gfx::Rect(0, 0, 500, 500));
  const DisplayInfo external_display_info =
      CreateDisplayInfo(external_id, gfx::Rect(1, 1, 100, 100));
  std::vector<DisplayInfo> display_info_list;

  display_manager->SetSecondDisplayMode(DisplayManager::MIRRORING);

  // software mirroring.
  display_info_list.push_back(internal_display_info);
  display_info_list.push_back(external_display_info);
  display_manager->OnNativeDisplaysChanged(display_info_list);
  const int64 internal_display_id =
      test::DisplayManagerTestApi(display_manager).
      SetFirstDisplayAsInternalDisplay();
  EXPECT_EQ(internal_id, internal_display_id);

  EXPECT_EQ(1U, display_manager->GetNumDisplays());
  EXPECT_TRUE(display_manager->IsMirrored());
  EXPECT_EQ(external_id, display_manager->mirrored_display_id());

  // dock mode.
  display_info_list.clear();
  display_info_list.push_back(external_display_info);
  display_manager->SetSecondDisplayMode(DisplayManager::MIRRORING);
  display_manager->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(1U, display_manager->GetNumDisplays());
  EXPECT_FALSE(display_manager->IsMirrored());

  // back to software mirroring.
  display_info_list.clear();
  display_info_list.push_back(internal_display_info);
  display_info_list.push_back(external_display_info);
  display_manager->SetSecondDisplayMode(DisplayManager::MIRRORING);
  display_manager->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(1U, display_manager->GetNumDisplays());
  EXPECT_TRUE(display_manager->IsMirrored());
  EXPECT_EQ(external_id, display_manager->mirrored_display_id());
}

TEST_F(MirrorOnBootTest, MAYBE_MirrorOnBoot) {
  DisplayManager* display_manager = Shell::GetInstance()->display_manager();
  EXPECT_TRUE(display_manager->IsMirrored());
  RunAllPendingInMessageLoop();
  test::MirrorWindowTestApi test_api;
  EXPECT_TRUE(test_api.GetHost());
}

}  // namespace ash
