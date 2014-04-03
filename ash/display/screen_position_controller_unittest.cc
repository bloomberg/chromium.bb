// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/screen_position_controller.h"

#include "ash/display/display_manager.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/shell_test_api.h"
#include "ui/aura/env.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/layout.h"
#include "ui/gfx/screen.h"

#if defined(OS_WIN)
// TODO(scottmg): RootWindow doesn't get resized immediately on Windows
// Ash. http://crbug.com/247916.
#define MAYBE_ConvertHostPointToScreen DISABLED_ConvertHostPointToScreen
#define MAYBE_ConvertHostPointToScreenHiDPI DISABLED_ConvertHostPointToScreenHiDPI
#define MAYBE_ConvertHostPointToScreenRotate DISABLED_ConvertHostPointToScreenRotate
#define MAYBE_ConvertHostPointToScreenUIScale DISABLED_ConvertHostPointToScreenUIScale
#else
#define MAYBE_ConvertHostPointToScreen ConvertHostPointToScreen
#define MAYBE_ConvertHostPointToScreenHiDPI ConvertHostPointToScreenHiDPI
#define MAYBE_ConvertHostPointToScreenRotate ConvertHostPointToScreenRotate
#define MAYBE_ConvertHostPointToScreenUIScale ConvertHostPointToScreenUIScale
#endif

namespace ash {
namespace test {

namespace {
void SetSecondaryDisplayLayout(DisplayLayout::Position position) {
  DisplayLayout layout =
      Shell::GetInstance()->display_manager()->GetCurrentDisplayLayout();
  layout.position = position;
  Shell::GetInstance()->display_manager()->
      SetLayoutForCurrentDisplays(layout);
}

ScreenPositionController* GetScreenPositionController() {
  ShellTestApi test_api(Shell::GetInstance());
  return test_api.screen_position_controller();
}

class ScreenPositionControllerTest : public test::AshTestBase {
 public:
  ScreenPositionControllerTest() {}
  virtual ~ScreenPositionControllerTest() {}

  virtual void SetUp() OVERRIDE {
    AshTestBase::SetUp();
    window_.reset(new aura::Window(&window_delegate_));
    window_->SetType(ui::wm::WINDOW_TYPE_NORMAL);
    window_->Init(aura::WINDOW_LAYER_NOT_DRAWN);
    ParentWindowInPrimaryRootWindow(window_.get());
    window_->set_id(1);
  }

  virtual void TearDown() OVERRIDE {
    window_.reset();
    AshTestBase::TearDown();
  }

  // Converts a point (x, y) in host window's coordinate to screen and
  // returns its string representation.
  std::string ConvertHostPointToScreen(int x, int y) const {
    gfx::Point point(x, y);
    GetScreenPositionController()->ConvertHostPointToScreen(
        window_->GetRootWindow(), &point);
    return point.ToString();
  }

 protected:
  scoped_ptr<aura::Window> window_;
  aura::test::TestWindowDelegate window_delegate_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenPositionControllerTest);
};

}  // namespace

TEST_F(ScreenPositionControllerTest, MAYBE_ConvertHostPointToScreen) {
  UpdateDisplay("100+100-200x200,100+500-200x200");

  aura::Window::Windows root_windows =
      Shell::GetInstance()->GetAllRootWindows();
  EXPECT_EQ("100,100",
            root_windows[0]->GetHost()->GetBounds().origin().ToString());
  EXPECT_EQ("200x200",
            root_windows[0]->GetHost()->GetBounds().size().ToString());
  EXPECT_EQ("100,500",
            root_windows[1]->GetHost()->GetBounds().origin().ToString());
  EXPECT_EQ("200x200",
            root_windows[1]->GetHost()->GetBounds().size().ToString());

  const gfx::Point window_pos(100, 100);
  window_->SetBoundsInScreen(
      gfx::Rect(window_pos, gfx::Size(100, 100)),
      Shell::GetScreen()->GetDisplayNearestPoint(window_pos));
  SetSecondaryDisplayLayout(DisplayLayout::RIGHT);
  // The point is on the primary root window.
  EXPECT_EQ("50,50", ConvertHostPointToScreen(50, 50));
  // The point is out of the all root windows.
  EXPECT_EQ("250,250", ConvertHostPointToScreen(250, 250));
  // The point is on the secondary display.
  EXPECT_EQ("250,0", ConvertHostPointToScreen(50, 400));

  SetSecondaryDisplayLayout(DisplayLayout::BOTTOM);
  // The point is on the primary root window.
  EXPECT_EQ("50,50", ConvertHostPointToScreen(50, 50));
  // The point is out of the all root windows.
  EXPECT_EQ("250,250", ConvertHostPointToScreen(250, 250));
  // The point is on the secondary display.
  EXPECT_EQ("50,200", ConvertHostPointToScreen(50, 400));

  SetSecondaryDisplayLayout(DisplayLayout::LEFT);
  // The point is on the primary root window.
  EXPECT_EQ("50,50", ConvertHostPointToScreen(50, 50));
  // The point is out of the all root windows.
  EXPECT_EQ("250,250", ConvertHostPointToScreen(250, 250));
  // The point is on the secondary display.
  EXPECT_EQ("-150,0", ConvertHostPointToScreen(50, 400));

  SetSecondaryDisplayLayout(DisplayLayout::TOP);
  // The point is on the primary root window.
  EXPECT_EQ("50,50", ConvertHostPointToScreen(50, 50));
  // The point is out of the all root windows.
  EXPECT_EQ("250,250", ConvertHostPointToScreen(250, 250));
  // The point is on the secondary display.
  EXPECT_EQ("50,-200", ConvertHostPointToScreen(50, 400));


  SetSecondaryDisplayLayout(DisplayLayout::RIGHT);
  const gfx::Point window_pos2(300, 100);
  window_->SetBoundsInScreen(
      gfx::Rect(window_pos2, gfx::Size(100, 100)),
      Shell::GetScreen()->GetDisplayNearestPoint(window_pos2));
  // The point is on the secondary display.
  EXPECT_EQ("250,50", ConvertHostPointToScreen(50, 50));
  // The point is out of the all root windows.
  EXPECT_EQ("450,250", ConvertHostPointToScreen(250, 250));
  // The point is on the primary root window.
  EXPECT_EQ("50,0", ConvertHostPointToScreen(50, -400));

  SetSecondaryDisplayLayout(DisplayLayout::BOTTOM);
  // The point is on the secondary display.
  EXPECT_EQ("50,250", ConvertHostPointToScreen(50, 50));
  // The point is out of the all root windows.
  EXPECT_EQ("250,450", ConvertHostPointToScreen(250, 250));
  // The point is on the primary root window.
  EXPECT_EQ("50,0", ConvertHostPointToScreen(50, -400));

  SetSecondaryDisplayLayout(DisplayLayout::LEFT);
  // The point is on the secondary display.
  EXPECT_EQ("-150,50", ConvertHostPointToScreen(50, 50));
  // The point is out of the all root windows.
  EXPECT_EQ("50,250", ConvertHostPointToScreen(250, 250));
  // The point is on the primary root window.
  EXPECT_EQ("50,0", ConvertHostPointToScreen(50, -400));

  SetSecondaryDisplayLayout(DisplayLayout::TOP);
  // The point is on the secondary display.
  EXPECT_EQ("50,-150", ConvertHostPointToScreen(50, 50));
  // The point is out of the all root windows.
  EXPECT_EQ("250,50", ConvertHostPointToScreen(250, 250));
  // The point is on the primary root window.
  EXPECT_EQ("50,0", ConvertHostPointToScreen(50, -400));
}

TEST_F(ScreenPositionControllerTest, MAYBE_ConvertHostPointToScreenHiDPI) {
  UpdateDisplay("100+100-200x200*2,100+500-200x200");

  aura::Window::Windows root_windows =
      Shell::GetInstance()->GetAllRootWindows();
  EXPECT_EQ("100,100",
            root_windows[0]->GetHost()->
                GetBounds().origin().ToString());
  EXPECT_EQ("200x200",
            root_windows[0]->GetHost()->
                GetBounds().size().ToString());
  EXPECT_EQ("100,500",
            root_windows[1]->GetHost()->
                GetBounds().origin().ToString());
  EXPECT_EQ("200x200",
            root_windows[1]->GetHost()->
                GetBounds().size().ToString());

  // Put |window_| to the primary 2x display.
  window_->SetBoundsInScreen(gfx::Rect(20, 20, 50, 50),
                             Shell::GetScreen()->GetPrimaryDisplay());
  // (30, 30) means the host coordinate, so the point is still on the primary
  // root window.  Since it's 2x, the specified native point was halved.
  EXPECT_EQ("15,15", ConvertHostPointToScreen(30, 30));
  // Similar to above but the point is out of the all root windows.
  EXPECT_EQ("200,200", ConvertHostPointToScreen(400, 400));
  // Similar to above but the point is on the secondary display.
  EXPECT_EQ("100,15", ConvertHostPointToScreen(200, 30));

  // On secondary display. The position on the 2nd host window is (150,50)
  // so the screen position is (100,0) + (150,50).
  EXPECT_EQ("250,50", ConvertHostPointToScreen(150, 450));

  // At the edge but still in the primary display.  Remaining of the primary
  // display is (50, 50) but adding ~100 since it's 2x-display.
  EXPECT_EQ("79,79", ConvertHostPointToScreen(158, 158));
  // At the edge of the secondary display.
  EXPECT_EQ("80,80", ConvertHostPointToScreen(160, 160));
}

TEST_F(ScreenPositionControllerTest, MAYBE_ConvertHostPointToScreenRotate) {
  // 1st display is rotated 90 clockise, and 2nd display is rotated
  // 270 clockwise.
  UpdateDisplay("100+100-200x200/r,100+500-200x200/l");
  // Put |window_| to the 1st.
  window_->SetBoundsInScreen(gfx::Rect(20, 20, 50, 50),
                             Shell::GetScreen()->GetPrimaryDisplay());

  // The point is on the 1st host.
  EXPECT_EQ("70,149", ConvertHostPointToScreen(50, 70));
  // The point is out of the host windows.
  EXPECT_EQ("250,-51", ConvertHostPointToScreen(250, 250));
  // The point is on the 2nd host. Point on 2nd host (30,150) -
  // rotate 270 clockwise -> (149, 30) - layout [+(200,0)] -> (349,30).
  EXPECT_EQ("349,30", ConvertHostPointToScreen(30, 450));

  // Move |window_| to the 2nd.
  window_->SetBoundsInScreen(gfx::Rect(300, 20, 50, 50),
                             ScreenUtil::GetSecondaryDisplay());
  aura::Window::Windows root_windows =
      Shell::GetInstance()->GetAllRootWindows();
  EXPECT_EQ(root_windows[1], window_->GetRootWindow());

  // The point is on the 2nd host. (50,70) on 2n host -
  // roatate 270 clockwise -> (129,50) -layout [+(200,0)] -> (329,50)
  EXPECT_EQ("329,50", ConvertHostPointToScreen(50, 70));
  // The point is out of the host windows.
  EXPECT_EQ("449,50", ConvertHostPointToScreen(50, -50));
  // The point is on the 2nd host. Point on 2nd host (50,50) -
  // rotate 90 clockwise -> (50, 149)
  EXPECT_EQ("50,149", ConvertHostPointToScreen(50, -350));
}

TEST_F(ScreenPositionControllerTest, MAYBE_ConvertHostPointToScreenUIScale) {
  // 1st display is 2x density with 1.5 UI scale.
  UpdateDisplay("100+100-200x200*2@1.5,100+500-200x200");
  // Put |window_| to the 1st.
  window_->SetBoundsInScreen(gfx::Rect(20, 20, 50, 50),
                             Shell::GetScreen()->GetPrimaryDisplay());

  // The point is on the 1st host.
  EXPECT_EQ("45,45", ConvertHostPointToScreen(60, 60));
  // The point is out of the host windows.
  EXPECT_EQ("45,225", ConvertHostPointToScreen(60, 300));
  // The point is on the 2nd host. Point on 2nd host (60,150) -
  // - screen [+(150,0)]
  EXPECT_EQ("210,49", ConvertHostPointToScreen(60, 450));

  // Move |window_| to the 2nd.
  window_->SetBoundsInScreen(gfx::Rect(300, 20, 50, 50),
                             ScreenUtil::GetSecondaryDisplay());
  aura::Window::Windows root_windows =
      Shell::GetInstance()->GetAllRootWindows();
  EXPECT_EQ(root_windows[1], window_->GetRootWindow());

  // The point is on the 2nd host. (50,70) - ro
  EXPECT_EQ("210,70", ConvertHostPointToScreen(60, 70));
  // The point is out of the host windows.
  EXPECT_EQ("210,-50", ConvertHostPointToScreen(60, -50));
  // The point is on the 2nd host. Point on 1nd host (60, 60)
  // 1/2 * 1.5 = (45,45)
  EXPECT_EQ("45,45", ConvertHostPointToScreen(60, -340));
}

}  // namespace test
}  // namespace ash
