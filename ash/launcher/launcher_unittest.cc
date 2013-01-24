// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/launcher.h"
#include "ash/launcher/launcher_button.h"
#include "ash/launcher/launcher_model.h"
#include "ash/launcher/launcher_view.h"

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

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

typedef ash::test::AshTestBase LauncherTest;
using ash::internal::LauncherView;
using ash::internal::LauncherButton;

namespace ash {

// Makes sure invoking SetStatusSize on the launcher changes the size of the
// LauncherView.
TEST_F(LauncherTest, SetStatusSize) {
  Launcher* launcher = Launcher::ForPrimaryDisplay();
  LauncherView* launcher_view = launcher->GetLauncherViewForTest();

  gfx::Size launcher_size =
      launcher->widget()->GetWindowBoundsInScreen().size();
  int total_width = launcher_size.width();
  ASSERT_GT(total_width, 0);
  launcher->SetStatusSize(gfx::Size(total_width / 2, launcher_size.height()));
  EXPECT_EQ(total_width - total_width / 2, launcher_view->width());
}

// Tests that the dimmer widget resizes itself as appropriate.
TEST_F(LauncherTest, DimmerSize) {
  Launcher* launcher = Launcher::ForPrimaryDisplay();
  launcher->SetDimsShelf(true);

  gfx::Size launcher_size =
      launcher->widget()->GetWindowBoundsInScreen().size();
  EXPECT_EQ(
      launcher->widget()->GetWindowBoundsInScreen().ToString(),
      launcher->GetDimmerWidgetForTest()->GetWindowBoundsInScreen().ToString());

  launcher->widget()->SetSize(
      gfx::Size(launcher_size.width() / 2, launcher_size.height() + 10));
  EXPECT_EQ(
      launcher->widget()->GetWindowBoundsInScreen().ToString(),
      launcher->GetDimmerWidgetForTest()->GetWindowBoundsInScreen().ToString());
}

// Confirm that launching a browser gets the appropriate state reflected in
// its button.
TEST_F(LauncherTest, OpenBrowser) {
  Launcher* launcher = Launcher::ForPrimaryDisplay();
  ASSERT_TRUE(launcher);
  LauncherView* launcher_view = launcher->GetLauncherViewForTest();
  test::LauncherViewTestAPI test(launcher_view);
  LauncherModel* model = launcher_view->model();

  // Initially we have the app list and chrome icon.
  int button_count = test.GetButtonCount();

  // Add running tab.
  LauncherItem item;
  item.type = TYPE_TABBED;
  item.status = STATUS_RUNNING;
  int index = model->Add(item);
  ASSERT_EQ(++button_count, test.GetButtonCount());
  LauncherButton* button = test.GetButton(index);
  EXPECT_EQ(LauncherButton::STATE_RUNNING, button->state());

  // Remove it.
  model->RemoveItemAt(index);
  ASSERT_EQ(--button_count, test.GetButtonCount());
}

TEST_F(LauncherTest, ShowOverflowBubble) {
  Launcher* launcher = Launcher::ForPrimaryDisplay();
  ASSERT_TRUE(launcher);

  LauncherView* launcher_view = launcher->GetLauncherViewForTest();
  test::LauncherViewTestAPI test(launcher_view);
  test.SetAnimationDuration(1);  // Speeds up animation for test.

  LauncherModel* model = launcher_view->model();

  // Add tabbed browser until overflow.
  int items_added = 0;
  while (!test.IsOverflowButtonVisible()) {
    LauncherItem item;
    item.type = TYPE_TABBED;
    item.status = STATUS_RUNNING;
    model->Add(item);

    ++items_added;
    ASSERT_LT(items_added, 10000);
  }

  // Shows overflow bubble.
  test.ShowOverflowBubble();
  EXPECT_TRUE(launcher->IsShowingOverflowBubble());
}

// Launcher can't be activated on mouse click, but it is activable from
// the focus cycler or as fallback.
TEST_F(LauncherTest, ActivateAsFallback) {
  // TODO(mtomasz): make this test work with the FocusController.
  if (views::corewm::UseFocusController())
    return;

  Launcher* launcher = Launcher::ForPrimaryDisplay();
  views::Widget* launcher_widget = launcher->widget();
  EXPECT_FALSE(launcher_widget->CanActivate());

  launcher->WillActivateAsFallback();
  EXPECT_TRUE(launcher_widget->CanActivate());

  wm::ActivateWindow(launcher_widget->GetNativeWindow());
  EXPECT_FALSE(launcher_widget->CanActivate());
}

void TestLauncherAlignment(aura::RootWindow* root,
                           ShelfAlignment alignment,
                           const std::string& expected) {
  Shell::GetInstance()->SetShelfAlignment(alignment, root);
  gfx::Screen* screen = gfx::Screen::GetScreenFor(root);
  EXPECT_EQ(expected,
            screen->GetDisplayNearestWindow(root).work_area().ToString());
}

TEST_F(LauncherTest, TestAlignment) {
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
                          "0,0 348x400");
  }
  {
    SCOPED_TRACE("Single Left");
    TestLauncherAlignment(Shell::GetPrimaryRootWindow(),
                          SHELF_ALIGNMENT_LEFT,
                          "52,0 348x400");
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
                          "0,0 248x300");
  }
  {
    SCOPED_TRACE("Primary Left");
    TestLauncherAlignment(root_windows[0],
                          SHELF_ALIGNMENT_LEFT,
                          "52,0 248x300");
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
                            "300,0 448x500");
    }
    {
      SCOPED_TRACE("Secondary Left");
      TestLauncherAlignment(root_windows[1],
                            SHELF_ALIGNMENT_LEFT,
                            "352,0 448x500");
    }
  }
}

}  // namespace ash
