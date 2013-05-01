// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_widget.h"

#include "ash/launcher/launcher.h"
#include "ash/launcher/launcher_button.h"
#include "ash/launcher/launcher_model.h"
#include "ash/launcher/launcher_view.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/launcher_view_test_api.h"
#include "ash/wm/window_util.h"
#include "ui/aura/root_window.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"
#include "ui/views/corewm/corewm_switches.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {
ShelfWidget* GetShelfWidget() {
  return Launcher::ForPrimaryDisplay()->shelf_widget();
}

internal::ShelfLayoutManager* GetShelfLayoutManager() {
  return GetShelfWidget()->shelf_layout_manager();
}

} // namespace

typedef test::AshTestBase ShelfWidgetTest;

// Launcher can't be activated on mouse click, but it is activable from
// the focus cycler or as fallback.
TEST_F(ShelfWidgetTest, ActivateAsFallback) {
  // TODO(mtomasz): make this test work with the FocusController.
  if (views::corewm::UseFocusController())
    return;

  Launcher* launcher = Launcher::ForPrimaryDisplay();
  ShelfWidget* shelf_widget = launcher->shelf_widget();
  EXPECT_FALSE(shelf_widget->CanActivate());

  shelf_widget->WillActivateAsFallback();
  EXPECT_TRUE(shelf_widget->CanActivate());

  wm::ActivateWindow(shelf_widget->GetNativeWindow());
  EXPECT_FALSE(shelf_widget->CanActivate());
}

void TestLauncherAlignment(aura::RootWindow* root,
                           ShelfAlignment alignment,
                           const std::string& expected) {
  Shell::GetInstance()->SetShelfAlignment(alignment, root);
  gfx::Screen* screen = gfx::Screen::GetScreenFor(root);
  EXPECT_EQ(expected,
            screen->GetDisplayNearestWindow(root).work_area().ToString());
}

TEST_F(ShelfWidgetTest, TestAlignment) {
  Launcher* launcher = Launcher::ForPrimaryDisplay();
  UpdateDisplay("400x400");
  ASSERT_TRUE(launcher);
  {
    SCOPED_TRACE("Single Bottom");
    TestLauncherAlignment(Shell::GetPrimaryRootWindow(),
                          SHELF_ALIGNMENT_BOTTOM,
                          "0,0 400x352");
  }
  {
    SCOPED_TRACE("Single Right");
    TestLauncherAlignment(Shell::GetPrimaryRootWindow(),
                          SHELF_ALIGNMENT_RIGHT,
                          "0,0 352x400");
  }
  {
    SCOPED_TRACE("Single Left");
    TestLauncherAlignment(Shell::GetPrimaryRootWindow(),
                          SHELF_ALIGNMENT_LEFT,
                          "48,0 352x400");
  }
  UpdateDisplay("300x300,500x500");
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  {
    SCOPED_TRACE("Primary Bottom");
    TestLauncherAlignment(root_windows[0],
                          SHELF_ALIGNMENT_BOTTOM,
                          "0,0 300x252");
  }
  {
    SCOPED_TRACE("Primary Right");
    TestLauncherAlignment(root_windows[0],
                          SHELF_ALIGNMENT_RIGHT,
                          "0,0 252x300");
  }
  {
    SCOPED_TRACE("Primary Left");
    TestLauncherAlignment(root_windows[0],
                          SHELF_ALIGNMENT_LEFT,
                          "48,0 252x300");
  }
  if (Shell::IsLauncherPerDisplayEnabled()) {
    {
      SCOPED_TRACE("Secondary Bottom");
      TestLauncherAlignment(root_windows[1],
                            SHELF_ALIGNMENT_BOTTOM,
                            "300,0 500x452");
    }
    {
      SCOPED_TRACE("Secondary Right");
      TestLauncherAlignment(root_windows[1],
                            SHELF_ALIGNMENT_RIGHT,
                            "300,0 452x500");
    }
    {
      SCOPED_TRACE("Secondary Left");
      TestLauncherAlignment(root_windows[1],
                            SHELF_ALIGNMENT_LEFT,
                            "348,0 452x500");
    }
  }
}

// Makes sure the launcher is initially sized correctly.
TEST_F(ShelfWidgetTest, LauncherInitiallySized) {
  ShelfWidget* shelf_widget = GetShelfWidget();
  Launcher* launcher = shelf_widget->launcher();
  ASSERT_TRUE(launcher);
  internal::ShelfLayoutManager* shelf_layout_manager = GetShelfLayoutManager();
  ASSERT_TRUE(shelf_layout_manager);
  ASSERT_TRUE(shelf_widget->status_area_widget());
  int status_width = shelf_widget->status_area_widget()->
      GetWindowBoundsInScreen().width();
  // Test only makes sense if the status is > 0, which it better be.
  EXPECT_GT(status_width, 0);
  EXPECT_EQ(status_width, shelf_widget->GetContentsView()->width() -
            launcher->GetLauncherViewForTest()->width());
}

// Verifies when the shell is deleted with a full screen window we don't crash.
TEST_F(ShelfWidgetTest, DontReferenceLauncherAfterDeletion) {
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(0, 0, 200, 200);
  params.context = CurrentContext();
  // Widget is now owned by the parent window.
  widget->Init(params);
  widget->SetFullscreen(true);
}

}  // namespace ash
