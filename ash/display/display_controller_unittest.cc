// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_controller.h"

#include "ash/display/display_info.h"
#include "ash/display/display_manager.h"
#include "ash/launcher/launcher.h"
#include "ash/screen_ash.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/cursor_manager_test_api.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/window_tracker.h"
#include "ui/base/events/event_handler.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace test {
namespace {

class TestObserver : public DisplayController::Observer {
 public:
  TestObserver() : changing_count_(0), changed_count_(0) {
    Shell::GetInstance()->display_controller()->AddObserver(this);
  }

  virtual ~TestObserver() {
    Shell::GetInstance()->display_controller()->RemoveObserver(this);
  }

  virtual void OnDisplayConfigurationChanging() OVERRIDE {
    ++changing_count_;
  }

  virtual void OnDisplayConfigurationChanged() OVERRIDE {
    ++changed_count_;
  }

  int CountAndReset() {
    EXPECT_EQ(changing_count_, changed_count_);
    int c = changing_count_;
    changing_count_ = changed_count_ = 0;
    return c;
  }

 private:
  int changing_count_;
  int changed_count_;

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
    gfx::Display primary = Shell::GetScreen()->GetPrimaryDisplay();
    EXPECT_EQ("0,0 444x333", primary.bounds().ToString());
    EXPECT_EQ(2, Shell::GetScreen()->GetNumDisplays());
  }
};

class TestEventHandler : public ui::EventHandler {
 public:
  TestEventHandler() : target_root_(NULL) {}
  virtual ~TestEventHandler() {}

  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE {
    aura::Window* target = static_cast<aura::Window*>(event->target());
    // Only record when the target is the background which covers
    // entire root window.
    if (target->name() != "DesktopBackgroundView")
      return;
    mouse_location_ = event->location();
    target_root_ = target->GetRootWindow();
    event->StopPropagation();
  }

  std::string GetLocationAndReset() {
    std::string result = mouse_location_.ToString();
    mouse_location_.SetPoint(0, 0);
    target_root_ = NULL;
    return result;
  }

 private:
  gfx::Point mouse_location_;
  aura::RootWindow* target_root_;

  DISALLOW_COPY_AND_ASSIGN(TestEventHandler);
};

gfx::Display::Rotation GetStoredRotation(int64 id) {
  return Shell::GetInstance()->display_manager()->GetDisplayInfo(id).rotation();
}

float GetStoredUIScale(int64 id) {
  return Shell::GetInstance()->display_manager()->GetDisplayInfo(id).ui_scale();
}

}  // namespace

typedef test::AshTestBase DisplayControllerTest;

TEST_F(DisplayControllerShutdownTest, Shutdown) {
  UpdateDisplay("444x333, 200x200");
}

TEST_F(DisplayControllerTest, SecondaryDisplayLayout) {
  TestObserver observer;
  UpdateDisplay("500x500,400x400");
  EXPECT_EQ(1, observer.CountAndReset());  // resize and add
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
  EXPECT_EQ(2, observer.CountAndReset());

  internal::DisplayManager* display_manager =
      Shell::GetInstance()->display_manager();
  gfx::Display* secondary_display = display_manager->GetDisplayAt(1);
  gfx::Insets insets(5, 5, 5, 5);
  secondary_display->UpdateWorkAreaFromInsets(insets);

  EXPECT_EQ("0,0 200x200", GetPrimaryDisplay().bounds().ToString());
  EXPECT_EQ("0,200 300x300", GetSecondaryDisplay().bounds().ToString());
  EXPECT_EQ("5,205 290x290", GetSecondaryDisplay().work_area().ToString());

  UpdateDisplay("400x400,200x200");
  EXPECT_EQ(1, observer.CountAndReset());  // two resizes
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

  UpdateDisplay("400x500,700x700*2");
  EXPECT_EQ(1, observer.CountAndReset());
  ASSERT_EQ(2, Shell::GetScreen()->GetNumDisplays());
  EXPECT_EQ("0,0 400x500", GetPrimaryDisplay().bounds().ToString());
  EXPECT_EQ("0,500 350x350", GetSecondaryDisplay().bounds().ToString());

  // No change
  UpdateDisplay("400x500,700x700*2");
  EXPECT_EQ(0, observer.CountAndReset());

  // Rotation
  int64 primary_id = GetPrimaryDisplay().id();
  display_manager->SetDisplayRotation(primary_id, gfx::Display::ROTATE_90);
  EXPECT_EQ(1, observer.CountAndReset());
  display_manager->SetDisplayRotation(primary_id, gfx::Display::ROTATE_90);
  EXPECT_EQ(0, observer.CountAndReset());

  // UI scale
  int64 secondary_id = GetSecondaryDisplay().id();
  gfx::Display::SetInternalDisplayId(secondary_id);
  display_manager->SetDisplayUIScale(secondary_id, 1.25f);
  EXPECT_EQ(1, observer.CountAndReset());
  display_manager->SetDisplayUIScale(secondary_id, 1.25f);
  EXPECT_EQ(0, observer.CountAndReset());
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

  DisplayLayout display_layout(DisplayLayout::RIGHT, 50);
  display_controller->SetLayoutForCurrentDisplays(display_layout);

  EXPECT_NE(primary_display.id(), secondary_display.id());
  aura::RootWindow* primary_root =
      display_controller->GetRootWindowForDisplayId(primary_display.id());
  aura::RootWindow* secondary_root =
      display_controller->GetRootWindowForDisplayId(secondary_display.id());
  EXPECT_NE(primary_root, secondary_root);
  aura::Window* launcher_window =
      Launcher::ForPrimaryDisplay()->shelf_widget()->GetNativeView();
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
  EXPECT_EQ("right, 50",
            display_controller->GetCurrentDisplayLayout().ToString());

  // Switch primary and secondary
  display_controller->SetPrimaryDisplay(secondary_display);
  const DisplayLayout& inverted_layout =
      display_controller->GetCurrentDisplayLayout();
  EXPECT_EQ("left, -50", inverted_layout.ToString());

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

  DisplayLayout display_layout(DisplayLayout::RIGHT, 50);
  display_controller->SetLayoutForCurrentDisplays(display_layout);

  EXPECT_NE(primary_display.id(), secondary_display.id());
  aura::RootWindow* primary_root =
      display_controller->GetRootWindowForDisplayId(primary_display.id());
  aura::RootWindow* secondary_root =
      display_controller->GetRootWindowForDisplayId(secondary_display.id());
  aura::Window* launcher_window =
      Launcher::ForPrimaryDisplay()->shelf_widget()->GetNativeView();
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
      display_controller->GetCurrentDisplayLayout();

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
  display_info_list.push_back(
      display_manager->GetDisplayInfo(primary_display.id()));
  display_info_list.push_back(
      display_manager->GetDisplayInfo(secondary_display.id()));
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
      display_manager->GetDisplayInfo(primary_display.id());
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
  EXPECT_EQ(1.0f, test_api.GetDisplay().device_scale_factor());
  EXPECT_EQ(2.0f,
            secondary_root->AsRootWindowHostDelegate()->GetDeviceScaleFactor());
  secondary_root->MoveCursorTo(gfx::Point(50, 50));
  EXPECT_EQ(2.0f, test_api.GetDisplay().device_scale_factor());

  // Switch primary and secondary
  display_controller->SetPrimaryDisplay(secondary_display);

  // Cursor's device scale factor should be updated accroding to the swap of
  // primary and secondary.
  EXPECT_EQ(1.0f,
            secondary_root->AsRootWindowHostDelegate()->GetDeviceScaleFactor());
  secondary_root->MoveCursorTo(gfx::Point(50, 50));
  EXPECT_EQ(1.0f, test_api.GetDisplay().device_scale_factor());
  primary_root->MoveCursorTo(gfx::Point(50, 50));
  EXPECT_EQ(2.0f,
            primary_root->AsRootWindowHostDelegate()->GetDeviceScaleFactor());
  EXPECT_EQ(2.0f, test_api.GetDisplay().device_scale_factor());

  // Deleting 2nd display.
  UpdateDisplay("200x200");
  RunAllPendingInMessageLoop();  // RootWindow is deleted in a posted task.

  // Cursor's device scale factor should be updated even without moving cursor.
  EXPECT_EQ(1.0f, test_api.GetDisplay().device_scale_factor());

  primary_root->MoveCursorTo(gfx::Point(50, 50));
  EXPECT_EQ(1.0f,
            primary_root->AsRootWindowHostDelegate()->GetDeviceScaleFactor());
  EXPECT_EQ(1.0f, test_api.GetDisplay().device_scale_factor());
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

#if defined(OS_WIN)
// TODO(oshima): Windows does not supoprts insets.
#define MAYBE_OverscanInsets DISABLED_OverscanInsets
#else
#define MAYBE_OverscanInsets OverscanInsets
#endif

TEST_F(DisplayControllerTest, MAYBE_OverscanInsets) {
  DisplayController* display_controller =
      Shell::GetInstance()->display_controller();
  TestEventHandler event_handler;
  Shell::GetInstance()->AddPreTargetHandler(&event_handler);

  UpdateDisplay("120x200,300x400*2");
  gfx::Display display1 = Shell::GetScreen()->GetPrimaryDisplay();
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();

  display_controller->SetOverscanInsets(display1.id(),
                                        gfx::Insets(10, 15, 20, 25));
  EXPECT_EQ("0,0 80x170", root_windows[0]->bounds().ToString());
  EXPECT_EQ("150x200", root_windows[1]->bounds().size().ToString());
  EXPECT_EQ("80,0 150x200",
            ScreenAsh::GetSecondaryDisplay().bounds().ToString());

  aura::test::EventGenerator generator(root_windows[0]);
  generator.MoveMouseTo(20, 25);
  EXPECT_EQ("5,15", event_handler.GetLocationAndReset());

  display_controller->ClearCustomOverscanInsets(display1.id());
  EXPECT_EQ("0,0 120x200", root_windows[0]->bounds().ToString());
  EXPECT_EQ("120,0 150x200",
            ScreenAsh::GetSecondaryDisplay().bounds().ToString());

  generator.MoveMouseTo(30, 20);
  EXPECT_EQ("30,20", event_handler.GetLocationAndReset());

  Shell::GetInstance()->RemovePreTargetHandler(&event_handler);
}

#if defined(OS_WIN)
// On Win8 bots, the host window can't be resized and
// SetTransform updates the window using the orignal host window
// size.
#define MAYBE_Rotate DISABLED_Rotate
#else
#define MAYBE_Rotate Rotate
#endif

TEST_F(DisplayControllerTest, MAYBE_Rotate) {
  DisplayController* display_controller =
      Shell::GetInstance()->display_controller();
  internal::DisplayManager* display_manager =
      Shell::GetInstance()->display_manager();
  TestEventHandler event_handler;
  Shell::GetInstance()->AddPreTargetHandler(&event_handler);

  UpdateDisplay("120x200,300x400*2");
  gfx::Display display1 = Shell::GetScreen()->GetPrimaryDisplay();
  int64 display2_id = ScreenAsh::GetSecondaryDisplay().id();
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  aura::test::EventGenerator generator1(root_windows[0]);

  EXPECT_EQ("120x200", root_windows[0]->bounds().size().ToString());
  EXPECT_EQ("150x200", root_windows[1]->bounds().size().ToString());
  EXPECT_EQ("120,0 150x200",
            ScreenAsh::GetSecondaryDisplay().bounds().ToString());
  generator1.MoveMouseTo(50, 40);
  EXPECT_EQ("50,40", event_handler.GetLocationAndReset());
  EXPECT_EQ(gfx::Display::ROTATE_0, GetStoredRotation(display1.id()));
  EXPECT_EQ(gfx::Display::ROTATE_0, GetStoredRotation(display2_id));

  display_manager->SetDisplayRotation(display1.id(),
                                      gfx::Display::ROTATE_90);
  EXPECT_EQ("200x120", root_windows[0]->bounds().size().ToString());
  EXPECT_EQ("150x200", root_windows[1]->bounds().size().ToString());
  EXPECT_EQ("200,0 150x200",
            ScreenAsh::GetSecondaryDisplay().bounds().ToString());
  generator1.MoveMouseTo(50, 40);
  EXPECT_EQ("40,69", event_handler.GetLocationAndReset());
  EXPECT_EQ(gfx::Display::ROTATE_90, GetStoredRotation(display1.id()));
  EXPECT_EQ(gfx::Display::ROTATE_0, GetStoredRotation(display2_id));

  DisplayLayout display_layout(DisplayLayout::BOTTOM, 50);
  display_controller->SetLayoutForCurrentDisplays(display_layout);
  EXPECT_EQ("50,120 150x200",
            ScreenAsh::GetSecondaryDisplay().bounds().ToString());

  display_manager->SetDisplayRotation(display2_id,
                                      gfx::Display::ROTATE_270);
  EXPECT_EQ("200x120", root_windows[0]->bounds().size().ToString());
  EXPECT_EQ("200x150", root_windows[1]->bounds().size().ToString());
  EXPECT_EQ("50,120 200x150",
            ScreenAsh::GetSecondaryDisplay().bounds().ToString());
  EXPECT_EQ(gfx::Display::ROTATE_90, GetStoredRotation(display1.id()));
  EXPECT_EQ(gfx::Display::ROTATE_270, GetStoredRotation(display2_id));

  aura::test::EventGenerator generator2(root_windows[1]);
  generator2.MoveMouseTo(50, 40);
  EXPECT_EQ("179,25", event_handler.GetLocationAndReset());
  display_manager->SetDisplayRotation(display1.id(),
                                      gfx::Display::ROTATE_180);

  EXPECT_EQ("120x200", root_windows[0]->bounds().size().ToString());
  EXPECT_EQ("200x150", root_windows[1]->bounds().size().ToString());
  // Dislay must share at least 100, so the x's offset becomes 20.
  EXPECT_EQ("20,200 200x150",
            ScreenAsh::GetSecondaryDisplay().bounds().ToString());
  EXPECT_EQ(gfx::Display::ROTATE_180, GetStoredRotation(display1.id()));
  EXPECT_EQ(gfx::Display::ROTATE_270, GetStoredRotation(display2_id));

  generator1.MoveMouseTo(50, 40);
  EXPECT_EQ("69,159", event_handler.GetLocationAndReset());

  Shell::GetInstance()->RemovePreTargetHandler(&event_handler);
}

#if defined(OS_WIN)
// On Win8 bots, the host window can't be resized and
// SetTransform updates the window using the orignal host window
// size.
#define MAYBE_ScaleRootWindow DISABLED_ScaleRootWindow
#else
#define MAYBE_ScaleRootWindow ScaleRootWindow
#endif

TEST_F(DisplayControllerTest, MAYBE_ScaleRootWindow) {
  TestEventHandler event_handler;
  Shell::GetInstance()->AddPreTargetHandler(&event_handler);

  UpdateDisplay("600x400*2@1.5,500x300");

  gfx::Display display1 = Shell::GetScreen()->GetPrimaryDisplay();
  gfx::Display display2 = ScreenAsh::GetSecondaryDisplay();
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  EXPECT_EQ("0,0 450x300", display1.bounds().ToString());
  EXPECT_EQ("0,0 450x300", root_windows[0]->bounds().ToString());
  EXPECT_EQ("450,0 500x300", display2.bounds().ToString());
  EXPECT_EQ(1.5f, GetStoredUIScale(display1.id()));
  EXPECT_EQ(1.0f, GetStoredUIScale(display2.id()));

  aura::test::EventGenerator generator(root_windows[0]);
  generator.MoveMouseTo(599, 200);
  EXPECT_EQ("449,150", event_handler.GetLocationAndReset());

  internal::DisplayManager* display_manager =
      Shell::GetInstance()->display_manager();
  display_manager->SetDisplayUIScale(display1.id(), 1.25);
  display1 = Shell::GetScreen()->GetPrimaryDisplay();
  display2 = ScreenAsh::GetSecondaryDisplay();
  EXPECT_EQ("0,0 375x250", display1.bounds().ToString());
  EXPECT_EQ("0,0 375x250", root_windows[0]->bounds().ToString());
  EXPECT_EQ("375,0 500x300", display2.bounds().ToString());
  EXPECT_EQ(1.25f, GetStoredUIScale(display1.id()));
  EXPECT_EQ(1.0f, GetStoredUIScale(display2.id()));

  Shell::GetInstance()->RemovePreTargetHandler(&event_handler);
}

}  // namespace test
}  // namespace ash
