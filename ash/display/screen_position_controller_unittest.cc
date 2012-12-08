// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/screen_position_controller.h"

#include "ash/display/display_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/shell_test_api.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/base/layout.h"
#include "ui/gfx/screen.h"

#if defined(OS_WIN)
// TOD(mazda): UpdateDisplay does not work properly on Win.
// Fix this and enable tests.
#define MAYBE_ConvertNativePointToScreen DISABLED_ConvertNativePointToScreen
#define MAYBE_ConvertNativePointToScreenHiDPI DISABLED_ConvertNativePointToScreenHiDPI
#else
#define MAYBE_ConvertNativePointToScreen ConvertNativePointToScreen
#define MAYBE_ConvertNativePointToScreenHiDPI ConvertNativePointToScreenHiDPI
#endif

namespace ash {
namespace test {

namespace {
void SetSecondaryDisplayLayout(DisplayLayout::Position position) {
  DisplayController* display_controller =
      Shell::GetInstance()->display_controller();
  DisplayLayout layout = display_controller->default_display_layout();
  layout.position = position;
  display_controller->SetDefaultDisplayLayout(layout);
}

internal::ScreenPositionController* GetScreenPositionController() {
  ShellTestApi test_api(Shell::GetInstance());
  return test_api.screen_position_controller();
}

class ScreenPositionControllerTest : public test::AshTestBase {
 public:
  ScreenPositionControllerTest() : window_(NULL) {}
  virtual ~ScreenPositionControllerTest() {}

  virtual void SetUp() OVERRIDE {
    AshTestBase::SetUp();
    window_.reset(new aura::Window(&window_delegate_));
    window_->SetType(aura::client::WINDOW_TYPE_NORMAL);
    window_->Init(ui::LAYER_NOT_DRAWN);
    SetDefaultParentByPrimaryRootWindow(window_.get());
    window_->set_id(1);
  }

  virtual void TearDown() OVERRIDE {
    window_.reset();
    AshTestBase::TearDown();
  }

  // Converts a native point (x, y) to screen and returns its string
  // representation.
  std::string ConvertNativePointToScreen(int x, int y) const {
    gfx::Point point(x, y);
    GetScreenPositionController()->ConvertNativePointToScreen(
        window_.get(), &point);
    return point.ToString();
  }

 protected:
  scoped_ptr<aura::Window> window_;
  aura::test::TestWindowDelegate window_delegate_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenPositionControllerTest);
};

}  // namespace

TEST_F(ScreenPositionControllerTest, MAYBE_ConvertNativePointToScreen) {
  UpdateDisplay("100+100-200x200,100+500-200x200");

  Shell::RootWindowList root_windows =
      Shell::GetInstance()->GetAllRootWindows();
  EXPECT_EQ("100,100", root_windows[0]->GetHostOrigin().ToString());
  EXPECT_EQ("200x200", root_windows[0]->GetHostSize().ToString());
  EXPECT_EQ("100,500", root_windows[1]->GetHostOrigin().ToString());
  EXPECT_EQ("200x200", root_windows[1]->GetHostSize().ToString());

  const gfx::Point window_pos(100, 100);
  window_->SetBoundsInScreen(
      gfx::Rect(window_pos, gfx::Size(100, 100)),
      Shell::GetScreen()->GetDisplayNearestPoint(window_pos));
  SetSecondaryDisplayLayout(DisplayLayout::RIGHT);
  // The point is on the primary root window.
  EXPECT_EQ("150,150", ConvertNativePointToScreen(50, 50));
  // The point is out of the all root windows.
  EXPECT_EQ("350,350", ConvertNativePointToScreen(250, 250));
  // The point is on the secondary display.
  EXPECT_EQ("350,100", ConvertNativePointToScreen(50, 400));

  SetSecondaryDisplayLayout(DisplayLayout::BOTTOM);
  // The point is on the primary root window.
  EXPECT_EQ("150,150", ConvertNativePointToScreen(50, 50));
  // The point is out of the all root windows.
  EXPECT_EQ("350,350", ConvertNativePointToScreen(250, 250));
  // The point is on the secondary display.
  EXPECT_EQ("150,300", ConvertNativePointToScreen(50, 400));

  SetSecondaryDisplayLayout(DisplayLayout::LEFT);
  // The point is on the primary root window.
  EXPECT_EQ("150,150", ConvertNativePointToScreen(50, 50));
  // The point is out of the all root windows.
  EXPECT_EQ("350,350", ConvertNativePointToScreen(250, 250));
  // The point is on the secondary display.
  EXPECT_EQ("-50,100", ConvertNativePointToScreen(50, 400));

  SetSecondaryDisplayLayout(DisplayLayout::TOP);
  // The point is on the primary root window.
  EXPECT_EQ("150,150", ConvertNativePointToScreen(50, 50));
  // The point is out of the all root windows.
  EXPECT_EQ("350,350", ConvertNativePointToScreen(250, 250));
  // The point is on the secondary display.
  EXPECT_EQ("150,-100", ConvertNativePointToScreen(50, 400));


  SetSecondaryDisplayLayout(DisplayLayout::RIGHT);
  const gfx::Point window_pos2(300, 100);
  window_->SetBoundsInScreen(
      gfx::Rect(window_pos2, gfx::Size(100, 100)),
      Shell::GetScreen()->GetDisplayNearestPoint(window_pos2));
  // The point is on the secondary display.
  EXPECT_EQ("350,150", ConvertNativePointToScreen(50, 50));
  // The point is out of the all root windows.
  EXPECT_EQ("550,350", ConvertNativePointToScreen(250, 250));
  // The point is on the primary root window.
  EXPECT_EQ("150,100", ConvertNativePointToScreen(50, -400));

  SetSecondaryDisplayLayout(DisplayLayout::BOTTOM);
  // The point is on the secondary display.
  EXPECT_EQ("150,350", ConvertNativePointToScreen(50, 50));
  // The point is out of the all root windows.
  EXPECT_EQ("350,550", ConvertNativePointToScreen(250, 250));
  // The point is on the primary root window.
  EXPECT_EQ("150,100", ConvertNativePointToScreen(50, -400));

  SetSecondaryDisplayLayout(DisplayLayout::LEFT);
  // The point is on the secondary display.
  EXPECT_EQ("-50,150", ConvertNativePointToScreen(50, 50));
  // The point is out of the all root windows.
  EXPECT_EQ("150,350", ConvertNativePointToScreen(250, 250));
  // The point is on the primary root window.
  EXPECT_EQ("150,100", ConvertNativePointToScreen(50, -400));

  SetSecondaryDisplayLayout(DisplayLayout::TOP);
  // The point is on the secondary display.
  EXPECT_EQ("150,-50", ConvertNativePointToScreen(50, 50));
  // The point is out of the all root windows.
  EXPECT_EQ("350,150", ConvertNativePointToScreen(250, 250));
  // The point is on the primary root window.
  EXPECT_EQ("150,100", ConvertNativePointToScreen(50, -400));
}

TEST_F(ScreenPositionControllerTest, MAYBE_ConvertNativePointToScreenHiDPI) {
  UpdateDisplay("100+100-200x200*2,100+500-200x200");

  Shell::RootWindowList root_windows =
      Shell::GetInstance()->GetAllRootWindows();
  EXPECT_EQ("100,100", root_windows[0]->GetHostOrigin().ToString());
  EXPECT_EQ("200x200", root_windows[0]->GetHostSize().ToString());
  EXPECT_EQ("100,500", root_windows[1]->GetHostOrigin().ToString());
  EXPECT_EQ("200x200", root_windows[1]->GetHostSize().ToString());

  ash::DisplayController* display_controller =
      Shell::GetInstance()->display_controller();
  // Put |window_| to the primary 2x display.
  window_->SetBoundsInScreen(gfx::Rect(20, 20, 50, 50),
                             display_controller->GetPrimaryDisplay());
  // (30, 30) means the native coordinate, so the point is still on the primary
  // root window.  Since it's 2x, the specified native point was halved.
  EXPECT_EQ("35,35", ConvertNativePointToScreen(30, 30));
  // Similar to above but the point is out of the all root windows.
  EXPECT_EQ("220,220", ConvertNativePointToScreen(400, 400));
  // Similar to above but the point is on the secondary display.
  EXPECT_EQ("120,35", ConvertNativePointToScreen(200, 30));
  // At the edge but still in the primary display.  Remaining of the primary
  // display is (50, 50) but adding ~100 since it's 2x-display.
  EXPECT_EQ("99,99", ConvertNativePointToScreen(159, 159));
  // At the edge of the secondary display.
  EXPECT_EQ("100,100", ConvertNativePointToScreen(160, 160));
}

}  // namespace test
}  // namespace ash
