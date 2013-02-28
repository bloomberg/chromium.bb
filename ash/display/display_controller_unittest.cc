// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_controller.h"

#include "ash/display/display_manager.h"
#include "ash/launcher/launcher.h"
#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/cursor_manager_test_api.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window_tracker.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace test {
namespace {

class TestObserver : public DisplayController::Observer {
 public:
  TestObserver() : count_(0) {
    Shell::GetInstance()->display_controller()->AddObserver(this);
  }

  virtual ~TestObserver() {
    Shell::GetInstance()->display_controller()->RemoveObserver(this);
  }

  virtual void OnDisplayConfigurationChanging() OVERRIDE {
    ++count_;
  }

  int CountAndReset() {
    int c = count_;
    count_ = 0;
    return c;
  }

 private:
  int count_;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

gfx::Display GetPrimaryDisplay() {
  return Shell::GetScreen()->GetDisplayNearestWindow(
      Shell::GetAllRootWindows()[0]);
}

gfx::Display GetSecondaryDisplay() {
  return Shell::GetScreen()->GetDisplayNearestWindow(
      Shell::GetAllRootWindows()[1]);
}

void SetSecondaryDisplayLayoutAndOffset(DisplayLayout::Position position,
                                        int offset) {
  DisplayController* display_controller =
      Shell::GetInstance()->display_controller();
  DisplayLayout layout = display_controller->default_display_layout();
  layout.position = position;
  layout.offset = offset;
  display_controller->SetDefaultDisplayLayout(layout);
}

void SetSecondaryDisplayLayout(DisplayLayout::Position position) {
  SetSecondaryDisplayLayoutAndOffset(position, 0);
}

class DisplayControllerShutdownTest : public test::AshTestBase {
 public:
  virtual void TearDown() OVERRIDE {
    test::AshTestBase::TearDown();
    // Make sure that primary display is accessible after shutdown.
    gfx::Display primary = gfx::Screen::GetNativeScreen()->GetPrimaryDisplay();
    EXPECT_EQ("0,0 444x333", primary.bounds().ToString());
    EXPECT_EQ(2, gfx::Screen::GetNativeScreen()->GetNumDisplays());
  }
};

}  // namespace

typedef test::AshTestBase DisplayControllerTest;

TEST_F(DisplayControllerShutdownTest, Shutdown) {
  UpdateDisplay("444x333, 200x200");
}

TEST_F(DisplayControllerTest, SecondaryDisplayLayout) {
  TestObserver observer;
  UpdateDisplay("500x500,400x400");
  EXPECT_EQ(2, observer.CountAndReset());  // resize and add
  gfx::Display* secondary_display =
      Shell::GetInstance()->display_manager()->GetDisplayAt(1);
  gfx::Insets insets(5, 5, 5, 5);
  secondary_display->UpdateWorkAreaFromInsets(insets);

  // Default layout is RIGHT.
  EXPECT_EQ("0,0 500x500", GetPrimaryDisplay().bounds().ToString());
  EXPECT_EQ("500,0 400x400", GetSecondaryDisplay().bounds().ToString());
  EXPECT_EQ("505,5 390x390", GetSecondaryDisplay().work_area().ToString());

  // Layout the secondary display to the bottom of the primary.
  SetSecondaryDisplayLayout(DisplayLayout::BOTTOM);
  EXPECT_EQ(1, observer.CountAndReset());
  EXPECT_EQ("0,0 500x500", GetPrimaryDisplay().bounds().ToString());
  EXPECT_EQ("0,500 400x400", GetSecondaryDisplay().bounds().ToString());
  EXPECT_EQ("5,505 390x390", GetSecondaryDisplay().work_area().ToString());

  // Layout the secondary display to the left of the primary.
  SetSecondaryDisplayLayout(DisplayLayout::LEFT);
  EXPECT_EQ(1, observer.CountAndReset());
  EXPECT_EQ("0,0 500x500", GetPrimaryDisplay().bounds().ToString());
  EXPECT_EQ("-400,0 400x400", GetSecondaryDisplay().bounds().ToString());
  EXPECT_EQ("-395,5 390x390", GetSecondaryDisplay().work_area().ToString());

  // Layout the secondary display to the top of the primary.
  SetSecondaryDisplayLayout(DisplayLayout::TOP);
  EXPECT_EQ(1, observer.CountAndReset());
  EXPECT_EQ("0,0 500x500", GetPrimaryDisplay().bounds().ToString());
  EXPECT_EQ("0,-400 400x400", GetSecondaryDisplay().bounds().ToString());
  EXPECT_EQ("5,-395 390x390", GetSecondaryDisplay().work_area().ToString());

  // Layout to the right with an offset.
  SetSecondaryDisplayLayoutAndOffset(DisplayLayout::RIGHT, 300);
  EXPECT_EQ(1, observer.CountAndReset());  // resize and add
  EXPECT_EQ("0,0 500x500", GetPrimaryDisplay().bounds().ToString());
  EXPECT_EQ("500,300 400x400", GetSecondaryDisplay().bounds().ToString());

  // Keep the minimum 100.
  SetSecondaryDisplayLayoutAndOffset(DisplayLayout::RIGHT, 490);
  EXPECT_EQ(1, observer.CountAndReset());  // resize and add
  EXPECT_EQ("0,0 500x500", GetPrimaryDisplay().bounds().ToString());
  EXPECT_EQ("500,400 400x400", GetSecondaryDisplay().bounds().ToString());

  SetSecondaryDisplayLayoutAndOffset(DisplayLayout::RIGHT, -400);
  EXPECT_EQ(1, observer.CountAndReset());  // resize and add
  EXPECT_EQ("0,0 500x500", GetPrimaryDisplay().bounds().ToString());
  EXPECT_EQ("500,-300 400x400", GetSecondaryDisplay().bounds().ToString());

  //  Layout to the bottom with an offset.
  SetSecondaryDisplayLayoutAndOffset(DisplayLayout::BOTTOM, -200);
  EXPECT_EQ(1, observer.CountAndReset());  // resize and add
  EXPECT_EQ("0,0 500x500", GetPrimaryDisplay().bounds().ToString());
  EXPECT_EQ("-200,500 400x400", GetSecondaryDisplay().bounds().ToString());

  // Keep the minimum 100.
  SetSecondaryDisplayLayoutAndOffset(DisplayLayout::BOTTOM, 490);
  EXPECT_EQ(1, observer.CountAndReset());  // resize and add
  EXPECT_EQ("0,0 500x500", GetPrimaryDisplay().bounds().ToString());
  EXPECT_EQ("400,500 400x400", GetSecondaryDisplay().bounds().ToString());

  SetSecondaryDisplayLayoutAndOffset(DisplayLayout::BOTTOM, -400);
  EXPECT_EQ(1, observer.CountAndReset());  // resize and add
  EXPECT_EQ("0,0 500x500", GetPrimaryDisplay().bounds().ToString());
  EXPECT_EQ("-300,500 400x400", GetSecondaryDisplay().bounds().ToString());
}

TEST_F(DisplayControllerTest, BoundsUpdated) {
  TestObserver observer;
  SetSecondaryDisplayLayout(DisplayLayout::BOTTOM);
  UpdateDisplay("200x200,300x300");  // layout, resize and add.
  EXPECT_EQ(3, observer.CountAndReset());

  gfx::Display* secondary_display =
      Shell::GetInstance()->display_manager()->GetDisplayAt(1);
  gfx::Insets insets(5, 5, 5, 5);
  secondary_display->UpdateWorkAreaFromInsets(insets);

  EXPECT_EQ("0,0 200x200", GetPrimaryDisplay().bounds().ToString());
  EXPECT_EQ("0,200 300x300", GetSecondaryDisplay().bounds().ToString());
  EXPECT_EQ("5,205 290x290", GetSecondaryDisplay().work_area().ToString());

  UpdateDisplay("400x400,200x200");
  EXPECT_EQ(2, observer.CountAndReset());  // two resizes
  EXPECT_EQ("0,0 400x400", GetPrimaryDisplay().bounds().ToString());
  EXPECT_EQ("0,400 200x200", GetSecondaryDisplay().bounds().ToString());
  if (!ash::Shell::IsLauncherPerDisplayEnabled())
    EXPECT_EQ("5,405 190x190", GetSecondaryDisplay().work_area().ToString());

  UpdateDisplay("400x400,300x300");
  EXPECT_EQ(1, observer.CountAndReset());
  EXPECT_EQ("0,0 400x400", GetPrimaryDisplay().bounds().ToString());
  EXPECT_EQ("0,400 300x300", GetSecondaryDisplay().bounds().ToString());
  if (!ash::Shell::IsLauncherPerDisplayEnabled())
    EXPECT_EQ("5,405 290x290", GetSecondaryDisplay().work_area().ToString());

  UpdateDisplay("400x400");
  EXPECT_EQ(1, observer.CountAndReset());
  EXPECT_EQ("0,0 400x400", GetPrimaryDisplay().bounds().ToString());
  EXPECT_EQ(1, Shell::GetScreen()->GetNumDisplays());

  UpdateDisplay("500x500,700x700");
  EXPECT_EQ(2, observer.CountAndReset());
  ASSERT_EQ(2, Shell::GetScreen()->GetNumDisplays());
  EXPECT_EQ("0,0 500x500", GetPrimaryDisplay().bounds().ToString());
  EXPECT_EQ("0,500 700x700", GetSecondaryDisplay().bounds().ToString());
}

TEST_F(DisplayControllerTest, InvertLayout) {
  EXPECT_EQ("left, 0",
            DisplayLayout(DisplayLayout::RIGHT, 0).Invert().ToString());
  EXPECT_EQ("left, -100",
            DisplayLayout(DisplayLayout::RIGHT, 100).Invert().ToString());
  EXPECT_EQ("left, 50",
            DisplayLayout(DisplayLayout::RIGHT, -50).Invert().ToString());

  EXPECT_EQ("right, 0",
            DisplayLayout(DisplayLayout::LEFT, 0).Invert().ToString());
  EXPECT_EQ("right, -90",
            DisplayLayout(DisplayLayout::LEFT, 90).Invert().ToString());
  EXPECT_EQ("right, 60",
            DisplayLayout(DisplayLayout::LEFT, -60).Invert().ToString());

  EXPECT_EQ("bottom, 0",
            DisplayLayout(DisplayLayout::TOP, 0).Invert().ToString());
  EXPECT_EQ("bottom, -80",
            DisplayLayout(DisplayLayout::TOP, 80).Invert().ToString());
  EXPECT_EQ("bottom, 70",
            DisplayLayout(DisplayLayout::TOP, -70).Invert().ToString());

  EXPECT_EQ("top, 0",
            DisplayLayout(DisplayLayout::BOTTOM, 0).Invert().ToString());
  EXPECT_EQ("top, -70",
            DisplayLayout(DisplayLayout::BOTTOM, 70).Invert().ToString());
  EXPECT_EQ("top, 80",
            DisplayLayout(DisplayLayout::BOTTOM, -80).Invert().ToString());
}

TEST_F(DisplayControllerTest, SwapPrimary) {
  DisplayController* display_controller =
      Shell::GetInstance()->display_controller();

  UpdateDisplay("200x200,300x300");
  gfx::Display primary_display = Shell::GetScreen()->GetPrimaryDisplay();
  gfx::Display secondary_display = ScreenAsh::GetSecondaryDisplay();

  DisplayLayout secondary_layout(DisplayLayout::RIGHT, 50);
  display_controller->SetLayoutForDisplayId(
      secondary_display.id(), secondary_layout);

  EXPECT_NE(primary_display.id(), secondary_display.id());
  aura::RootWindow* primary_root =
      display_controller->GetRootWindowForDisplayId(primary_display.id());
  aura::RootWindow* secondary_root =
      display_controller->GetRootWindowForDisplayId(secondary_display.id());
  EXPECT_NE(primary_root, secondary_root);
  aura::Window* launcher_window =
      Launcher::ForPrimaryDisplay()->widget()->GetNativeView();
  EXPECT_TRUE(primary_root->Contains(launcher_window));
  EXPECT_FALSE(secondary_root->Contains(launcher_window));
  EXPECT_EQ(primary_display.id(),
            Shell::GetScreen()->GetDisplayNearestPoint(
                gfx::Point(-100, -100)).id());
  EXPECT_EQ(primary_display.id(),
            Shell::GetScreen()->GetDisplayNearestWindow(NULL).id());

  EXPECT_EQ("0,0 200x200", primary_display.bounds().ToString());
  EXPECT_EQ("0,0 200x152", primary_display.work_area().ToString());
  EXPECT_EQ("200,0 300x300", secondary_display.bounds().ToString());
  if (ash::Shell::IsLauncherPerDisplayEnabled())
    EXPECT_EQ("200,0 300x252", secondary_display.work_area().ToString());
  else
    EXPECT_EQ("200,0 300x300", secondary_display.work_area().ToString());

  // Switch primary and secondary
  display_controller->SetPrimaryDisplay(secondary_display);
  EXPECT_EQ(secondary_display.id(),
      Shell::GetScreen()->GetPrimaryDisplay().id());
  EXPECT_EQ(primary_display.id(), ScreenAsh::GetSecondaryDisplay().id());
  EXPECT_EQ(secondary_display.id(),
            Shell::GetScreen()->GetDisplayNearestPoint(
                gfx::Point(-100, -100)).id());
  EXPECT_EQ(secondary_display.id(),
            Shell::GetScreen()->GetDisplayNearestWindow(NULL).id());

  EXPECT_EQ(
      primary_root,
      display_controller->GetRootWindowForDisplayId(secondary_display.id()));
  EXPECT_EQ(
      secondary_root,
      display_controller->GetRootWindowForDisplayId(primary_display.id()));
  EXPECT_TRUE(primary_root->Contains(launcher_window));
  EXPECT_FALSE(secondary_root->Contains(launcher_window));

  // Test if the bounds are correctly swapped.
  gfx::Display swapped_primary = Shell::GetScreen()->GetPrimaryDisplay();
  gfx::Display swapped_secondary = ScreenAsh::GetSecondaryDisplay();
  EXPECT_EQ("0,0 300x300", swapped_primary.bounds().ToString());
  EXPECT_EQ("0,0 300x252", swapped_primary.work_area().ToString());
  EXPECT_EQ("-200,-50 200x200", swapped_secondary.bounds().ToString());

  if (ash::Shell::IsLauncherPerDisplayEnabled())
    EXPECT_EQ("-200,-50 200x152", swapped_secondary.work_area().ToString());
  else
    EXPECT_EQ("-200,-50 200x200", swapped_secondary.work_area().ToString());

  const DisplayLayout& inverted_layout =
      display_controller->GetLayoutForDisplay(primary_display);

  EXPECT_EQ("left, -50", inverted_layout.ToString());

  aura::WindowTracker tracker;
  tracker.Add(primary_root);
  tracker.Add(secondary_root);

  // Deleting 2nd display should move the primary to original primary display.
  UpdateDisplay("200x200");
  RunAllPendingInMessageLoop();  // RootWindow is deleted in a posted task.
  EXPECT_EQ(1, Shell::GetScreen()->GetNumDisplays());
  EXPECT_EQ(primary_display.id(), Shell::GetScreen()->GetPrimaryDisplay().id());
  EXPECT_EQ(primary_display.id(),
            Shell::GetScreen()->GetDisplayNearestPoint(
                gfx::Point(-100, -100)).id());
  EXPECT_EQ(primary_display.id(),
            Shell::GetScreen()->GetDisplayNearestWindow(NULL).id());
  EXPECT_TRUE(tracker.Contains(primary_root));
  EXPECT_FALSE(tracker.Contains(secondary_root));
  EXPECT_TRUE(primary_root->Contains(launcher_window));
}

TEST_F(DisplayControllerTest, SwapPrimaryById) {
  DisplayController* display_controller =
      Shell::GetInstance()->display_controller();

  UpdateDisplay("200x200,300x300");
  gfx::Display primary_display = Shell::GetScreen()->GetPrimaryDisplay();
  gfx::Display secondary_display = ScreenAsh::GetSecondaryDisplay();

  DisplayLayout secondary_layout(DisplayLayout::RIGHT, 50);
  display_controller->SetLayoutForDisplayId(
      secondary_display.id(), secondary_layout);

  EXPECT_NE(primary_display.id(), secondary_display.id());
  aura::RootWindow* primary_root =
      display_controller->GetRootWindowForDisplayId(primary_display.id());
  aura::RootWindow* secondary_root =
      display_controller->GetRootWindowForDisplayId(secondary_display.id());
  aura::Window* launcher_window =
      Launcher::ForPrimaryDisplay()->widget()->GetNativeView();
  EXPECT_TRUE(primary_root->Contains(launcher_window));
  EXPECT_FALSE(secondary_root->Contains(launcher_window));
  EXPECT_NE(primary_root, secondary_root);
  EXPECT_EQ(primary_display.id(),
            Shell::GetScreen()->GetDisplayNearestPoint(
                gfx::Point(-100, -100)).id());
  EXPECT_EQ(primary_display.id(),
            Shell::GetScreen()->GetDisplayNearestWindow(NULL).id());

  // Switch primary and secondary by display ID.
  TestObserver observer;
  display_controller->SetPrimaryDisplayId(secondary_display.id());
  EXPECT_EQ(secondary_display.id(),
            Shell::GetScreen()->GetPrimaryDisplay().id());
  EXPECT_EQ(primary_display.id(), ScreenAsh::GetSecondaryDisplay().id());
  EXPECT_LT(0, observer.CountAndReset());

  EXPECT_EQ(
      primary_root,
      display_controller->GetRootWindowForDisplayId(secondary_display.id()));
  EXPECT_EQ(
      secondary_root,
      display_controller->GetRootWindowForDisplayId(primary_display.id()));
  EXPECT_TRUE(primary_root->Contains(launcher_window));
  EXPECT_FALSE(secondary_root->Contains(launcher_window));

  const DisplayLayout& inverted_layout =
      display_controller->GetLayoutForDisplay(primary_display);

  EXPECT_EQ("left, -50", inverted_layout.ToString());

  // Calling the same ID don't do anything.
  display_controller->SetPrimaryDisplayId(secondary_display.id());
  EXPECT_EQ(0, observer.CountAndReset());

  aura::WindowTracker tracker;
  tracker.Add(primary_root);
  tracker.Add(secondary_root);

  // Deleting 2nd display should move the primary to original primary display.
  UpdateDisplay("200x200");
  RunAllPendingInMessageLoop();  // RootWindow is deleted in a posted task.
  EXPECT_EQ(1, Shell::GetScreen()->GetNumDisplays());
  EXPECT_EQ(primary_display.id(), Shell::GetScreen()->GetPrimaryDisplay().id());
  EXPECT_EQ(primary_display.id(),
            Shell::GetScreen()->GetDisplayNearestPoint(
                gfx::Point(-100, -100)).id());
  EXPECT_EQ(primary_display.id(),
            Shell::GetScreen()->GetDisplayNearestWindow(NULL).id());
  EXPECT_TRUE(tracker.Contains(primary_root));
  EXPECT_FALSE(tracker.Contains(secondary_root));
  EXPECT_TRUE(primary_root->Contains(launcher_window));

  internal::DisplayManager* display_manager =
      Shell::GetInstance()->display_manager();
  // Adding 2nd display with the same ID.  The 2nd display should become primary
  // since secondary id is still stored as desirable_primary_id.
  std::vector<internal::DisplayInfo> display_info_list;
  display_info_list.push_back(display_manager->GetDisplayInfo(primary_display));
  display_info_list.push_back(
      display_manager->GetDisplayInfo(secondary_display));
  display_manager->OnNativeDisplaysChanged(display_info_list);

  EXPECT_EQ(2, Shell::GetScreen()->GetNumDisplays());
  EXPECT_EQ(secondary_display.id(),
            Shell::GetScreen()->GetPrimaryDisplay().id());
  EXPECT_EQ(primary_display.id(), ScreenAsh::GetSecondaryDisplay().id());
  EXPECT_EQ(
      primary_root,
      display_controller->GetRootWindowForDisplayId(secondary_display.id()));
  EXPECT_NE(
      primary_root,
      display_controller->GetRootWindowForDisplayId(primary_display.id()));
  EXPECT_TRUE(primary_root->Contains(launcher_window));

  // Deleting 2nd display and adding 2nd display with a different ID.  The 2nd
  // display shouldn't become primary.
  UpdateDisplay("200x200");
  internal::DisplayInfo third_display_info(
      secondary_display.id() + 1, std::string(), false);
  third_display_info.SetBounds(secondary_display.bounds());
  ASSERT_NE(primary_display.id(), third_display_info.id());

  const internal::DisplayInfo& primary_display_info =
      display_manager->GetDisplayInfo(primary_display);
  std::vector<internal::DisplayInfo> display_info_list2;
  display_info_list2.push_back(primary_display_info);
  display_info_list2.push_back(third_display_info);
  display_manager->OnNativeDisplaysChanged(display_info_list2);
  EXPECT_EQ(2, Shell::GetScreen()->GetNumDisplays());
  EXPECT_EQ(primary_display.id(),
            Shell::GetScreen()->GetPrimaryDisplay().id());
  EXPECT_EQ(third_display_info.id(), ScreenAsh::GetSecondaryDisplay().id());
  EXPECT_EQ(
      primary_root,
      display_controller->GetRootWindowForDisplayId(primary_display.id()));
  EXPECT_NE(
      primary_root,
      display_controller->GetRootWindowForDisplayId(third_display_info.id()));
  EXPECT_TRUE(primary_root->Contains(launcher_window));
}

TEST_F(DisplayControllerTest, CursorDeviceScaleFactorSwapPrimary) {
  DisplayController* display_controller =
      Shell::GetInstance()->display_controller();

  UpdateDisplay("200x200,200x200*2");
  gfx::Display primary_display = Shell::GetScreen()->GetPrimaryDisplay();
  gfx::Display secondary_display = ScreenAsh::GetSecondaryDisplay();

  aura::RootWindow* primary_root =
      display_controller->GetRootWindowForDisplayId(primary_display.id());
  aura::RootWindow* secondary_root =
      display_controller->GetRootWindowForDisplayId(secondary_display.id());
  EXPECT_NE(primary_root, secondary_root);

  test::CursorManagerTestApi test_api(Shell::GetInstance()->cursor_manager());

  EXPECT_EQ(1.0f,
            primary_root->AsRootWindowHostDelegate()->GetDeviceScaleFactor());
  primary_root->MoveCursorTo(gfx::Point(50, 50));
  EXPECT_EQ(1.0f, test_api.GetDeviceScaleFactor());
  EXPECT_EQ(2.0f,
            secondary_root->AsRootWindowHostDelegate()->GetDeviceScaleFactor());
  secondary_root->MoveCursorTo(gfx::Point(50, 50));
  EXPECT_EQ(2.0f, test_api.GetDeviceScaleFactor());

  // Switch primary and secondary
  display_controller->SetPrimaryDisplay(secondary_display);

  // Cursor's device scale factor should be updated accroding to the swap of
  // primary and secondary.
  EXPECT_EQ(1.0f,
            secondary_root->AsRootWindowHostDelegate()->GetDeviceScaleFactor());
  secondary_root->MoveCursorTo(gfx::Point(50, 50));
  EXPECT_EQ(1.0f, test_api.GetDeviceScaleFactor());
  primary_root->MoveCursorTo(gfx::Point(50, 50));
  EXPECT_EQ(2.0f,
            primary_root->AsRootWindowHostDelegate()->GetDeviceScaleFactor());
  EXPECT_EQ(2.0f, test_api.GetDeviceScaleFactor());

  // Deleting 2nd display.
  UpdateDisplay("200x200");
  RunAllPendingInMessageLoop();  // RootWindow is deleted in a posted task.

  // Cursor's device scale factor should be updated even without moving cursor.
  EXPECT_EQ(1.0f, test_api.GetDeviceScaleFactor());

  primary_root->MoveCursorTo(gfx::Point(50, 50));
  EXPECT_EQ(1.0f,
            primary_root->AsRootWindowHostDelegate()->GetDeviceScaleFactor());
  EXPECT_EQ(1.0f, test_api.GetDeviceScaleFactor());
}

#if defined(OS_WIN)
// TODO(oshima): On Windows, we don't update the origin/size right away.
#define MAYBE_UpdateDisplayWithHostOrigin DISABLED_UpdateDisplayWithHostOrigin
#else
#define MAYBE_UpdateDisplayWithHostOrigin UpdateDisplayWithHostOrigin
#endif

TEST_F(DisplayControllerTest, MAYBE_UpdateDisplayWithHostOrigin) {
  UpdateDisplay("100x200,300x400");
  ASSERT_EQ(2, Shell::GetScreen()->GetNumDisplays());
  Shell::RootWindowList root_windows =
      Shell::GetInstance()->GetAllRootWindows();
  ASSERT_EQ(2U, root_windows.size());
  EXPECT_EQ("1,1", root_windows[0]->GetHostOrigin().ToString());
  EXPECT_EQ("100x200", root_windows[0]->GetHostSize().ToString());
  // UpdateDisplay set the origin if it's not set.
  EXPECT_NE("1,1", root_windows[1]->GetHostOrigin().ToString());
  EXPECT_EQ("300x400", root_windows[1]->GetHostSize().ToString());

  UpdateDisplay("100x200,200+300-300x400");
  ASSERT_EQ(2, Shell::GetScreen()->GetNumDisplays());
  EXPECT_EQ("0,0", root_windows[0]->GetHostOrigin().ToString());
  EXPECT_EQ("100x200", root_windows[0]->GetHostSize().ToString());
  EXPECT_EQ("200,300", root_windows[1]->GetHostOrigin().ToString());
  EXPECT_EQ("300x400", root_windows[1]->GetHostSize().ToString());

  UpdateDisplay("400+500-200x300,300x400");
  ASSERT_EQ(2, Shell::GetScreen()->GetNumDisplays());
  EXPECT_EQ("400,500", root_windows[0]->GetHostOrigin().ToString());
  EXPECT_EQ("200x300", root_windows[0]->GetHostSize().ToString());
  EXPECT_EQ("0,0", root_windows[1]->GetHostOrigin().ToString());
  EXPECT_EQ("300x400", root_windows[1]->GetHostSize().ToString());

  UpdateDisplay("100+200-100x200,300+500-200x300");
  ASSERT_EQ(2, Shell::GetScreen()->GetNumDisplays());
  EXPECT_EQ("100,200", root_windows[0]->GetHostOrigin().ToString());
  EXPECT_EQ("100x200", root_windows[0]->GetHostSize().ToString());
  EXPECT_EQ("300,500", root_windows[1]->GetHostOrigin().ToString());
  EXPECT_EQ("200x300", root_windows[1]->GetHostSize().ToString());
}

}  // namespace test
}  // namespace ash
