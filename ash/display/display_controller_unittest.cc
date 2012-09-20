// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_controller.h"

#include "ash/launcher/launcher.h"
#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ui/aura/display_manager.h"
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
  return gfx::Screen::GetDisplayNearestWindow(
      Shell::GetAllRootWindows()[0]);
}

gfx::Display GetSecondaryDisplay() {
  return gfx::Screen::GetDisplayNearestWindow(
      Shell::GetAllRootWindows()[1]);
}

void SetSecondaryDisplayLayout(DisplayLayout::Position position) {
  DisplayController* display_controller =
      Shell::GetInstance()->display_controller();
  DisplayLayout layout = display_controller->default_display_layout();
  layout.position = position;
  display_controller->SetDefaultDisplayLayout(layout);
}

}  // namespace

typedef test::AshTestBase DisplayControllerTest;

#if defined(OS_WIN)
// TOD(oshima): Windows creates a window with smaller client area.
// Fix this and enable tests.
#define MAYBE_SecondaryDisplayLayout DISABLED_SecondaryDisplayLayout
#define MAYBE_BoundsUpdated DISABLED_BoundsUpdated
#define MAYBE_UpdateDisplayWithHostOrigin DISABLED_UpdateDisplayWithHostOrigin
#else
#define MAYBE_SecondaryDisplayLayout SecondaryDisplayLayout
#define MAYBE_BoundsUpdated BoundsUpdated
#define MAYBE_UpdateDisplayWithHostOrigin UpdateDisplayWithHostOrigin
#endif

TEST_F(DisplayControllerTest, MAYBE_SecondaryDisplayLayout) {
  TestObserver observer;
  UpdateDisplay("500x500,400x400");
  EXPECT_EQ(2, observer.CountAndReset());  // resize and add
  gfx::Display* secondary_display =
      aura::Env::GetInstance()->display_manager()->GetDisplayAt(1);
  gfx::Insets insets(5, 5, 5, 5);
  secondary_display->UpdateWorkAreaFromInsets(insets);

  // Default layout is LEFT.
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
}

TEST_F(DisplayControllerTest, MAYBE_BoundsUpdated) {
  TestObserver observer;
  SetSecondaryDisplayLayout(DisplayLayout::BOTTOM);
  UpdateDisplay("200x200,300x300");  // layout, resize and add.
  EXPECT_EQ(3, observer.CountAndReset());

  gfx::Display* secondary_display =
      aura::Env::GetInstance()->display_manager()->GetDisplayAt(1);
  gfx::Insets insets(5, 5, 5, 5);
  secondary_display->UpdateWorkAreaFromInsets(insets);

  EXPECT_EQ("0,0 200x200", GetPrimaryDisplay().bounds().ToString());
  EXPECT_EQ("0,200 300x300", GetSecondaryDisplay().bounds().ToString());
  EXPECT_EQ("5,205 290x290", GetSecondaryDisplay().work_area().ToString());

  UpdateDisplay("400x400,200x200");
  EXPECT_EQ(2, observer.CountAndReset());  // two resizes
  EXPECT_EQ("0,0 400x400", GetPrimaryDisplay().bounds().ToString());
  EXPECT_EQ("0,400 200x200", GetSecondaryDisplay().bounds().ToString());
  EXPECT_EQ("5,405 190x190", GetSecondaryDisplay().work_area().ToString());

  UpdateDisplay("400x400,300x300");
  EXPECT_EQ(1, observer.CountAndReset());
  EXPECT_EQ("0,0 400x400", GetPrimaryDisplay().bounds().ToString());
  EXPECT_EQ("0,400 300x300", GetSecondaryDisplay().bounds().ToString());
  EXPECT_EQ("5,405 290x290", GetSecondaryDisplay().work_area().ToString());

  UpdateDisplay("400x400");
  EXPECT_EQ(1, observer.CountAndReset());
  EXPECT_EQ("0,0 400x400", GetPrimaryDisplay().bounds().ToString());
  EXPECT_EQ(1, gfx::Screen::GetNumDisplays());

  UpdateDisplay("500x500,700x700");
  EXPECT_EQ(2, observer.CountAndReset());
  ASSERT_EQ(2, gfx::Screen::GetNumDisplays());
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
  gfx::Display primary_display = gfx::Screen::GetPrimaryDisplay();
  gfx::Display secondary_display = ScreenAsh::GetSecondaryDisplay();

  std::string secondary_name = aura::Env::GetInstance()->
      display_manager()->GetDisplayNameFor(secondary_display);
  DisplayLayout secondary_layout(DisplayLayout::RIGHT, 50);
  display_controller->SetLayoutForDisplayName(secondary_name, secondary_layout);

  EXPECT_NE(primary_display.id(), secondary_display.id());
  aura::RootWindow* primary_root =
      display_controller->GetRootWindowForDisplayId(primary_display.id());
  aura::RootWindow* secondary_root =
      display_controller->GetRootWindowForDisplayId(secondary_display.id());
  EXPECT_NE(primary_root, secondary_root);
  aura::Window* launcher_window =
      Shell::GetInstance()->launcher()->widget()->GetNativeView();
  EXPECT_TRUE(primary_root->Contains(launcher_window));
  EXPECT_FALSE(secondary_root->Contains(launcher_window));

  // Switch primary and secondary
  display_controller->SetPrimaryDisplay(secondary_display);
  EXPECT_EQ(secondary_display.id(), gfx::Screen::GetPrimaryDisplay().id());
  EXPECT_EQ(primary_display.id(), ScreenAsh::GetSecondaryDisplay().id());

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

  aura::WindowTracker tracker;
  tracker.Add(primary_root);
  tracker.Add(secondary_root);

  // Deleting 2nd display should move the primary to original primary display.
  UpdateDisplay("200x200");
  RunAllPendingInMessageLoop();  // RootWindow is deleted in a posted task.
  EXPECT_EQ(1, gfx::Screen::GetNumDisplays());
  EXPECT_EQ(primary_display.id(), gfx::Screen::GetPrimaryDisplay().id());
  EXPECT_TRUE(tracker.Contains(primary_root));
  EXPECT_FALSE(tracker.Contains(secondary_root));
  EXPECT_TRUE(primary_root->Contains(launcher_window));
}

TEST_F(DisplayControllerTest, MAYBE_UpdateDisplayWithHostOrigin) {
  UpdateDisplay("100x200,300x400");
  ASSERT_EQ(2, gfx::Screen::GetNumDisplays());
  Shell::RootWindowList root_windows =
      Shell::GetInstance()->GetAllRootWindows();
  ASSERT_EQ(2U, root_windows.size());
  EXPECT_EQ("0,0", root_windows[0]->GetHostOrigin().ToString());
  EXPECT_EQ("100x200", root_windows[0]->GetHostSize().ToString());
  // UpdateDisplay set the origin if it's not set.
  EXPECT_NE("0,0", root_windows[1]->GetHostOrigin().ToString());
  EXPECT_EQ("300x400", root_windows[1]->GetHostSize().ToString());

  UpdateDisplay("100x200,200+300-300x400");
  ASSERT_EQ(2, gfx::Screen::GetNumDisplays());
  EXPECT_EQ("0,0", root_windows[0]->GetHostOrigin().ToString());
  EXPECT_EQ("100x200", root_windows[0]->GetHostSize().ToString());
  EXPECT_EQ("200,300", root_windows[1]->GetHostOrigin().ToString());
  EXPECT_EQ("300x400", root_windows[1]->GetHostSize().ToString());

  UpdateDisplay("400+500-200x300,300x400");
  ASSERT_EQ(2, gfx::Screen::GetNumDisplays());
  EXPECT_EQ("400,500", root_windows[0]->GetHostOrigin().ToString());
  EXPECT_EQ("200x300", root_windows[0]->GetHostSize().ToString());
  EXPECT_EQ("0,0", root_windows[1]->GetHostOrigin().ToString());
  EXPECT_EQ("300x400", root_windows[1]->GetHostSize().ToString());

  UpdateDisplay("100+200-100x200,300+500-200x300");
  ASSERT_EQ(2, gfx::Screen::GetNumDisplays());
  EXPECT_EQ("100,200", root_windows[0]->GetHostOrigin().ToString());
  EXPECT_EQ("100x200", root_windows[0]->GetHostSize().ToString());
  EXPECT_EQ("300,500", root_windows[1]->GetHostOrigin().ToString());
  EXPECT_EQ("200x300", root_windows[1]->GetHostSize().ToString());
}

}  // namespace test
}  // namespace ash
