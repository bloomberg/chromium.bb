// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/launcher.h"

#include <vector>

#include "ash/launcher/app_list_button.h"
#include "ash/launcher/launcher_button.h"
#include "ash/launcher/launcher_model.h"
#include "ash/launcher/launcher_view.h"
#include "ash/shelf/shelf_widget.h"
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
using ash::internal::AppListButton;
using ash::internal::LauncherView;
using ash::internal::LauncherButton;

namespace ash {

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

// Confirm that using the menu will clear the hover attribute. To avoid another
// browser test we check this here.
TEST_F(LauncherTest, CheckHoverAfterMenu) {
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
  button->AddState(LauncherButton::STATE_HOVERED);
  button->ShowContextMenu(gfx::Point(), true);
  EXPECT_FALSE(button->state() & LauncherButton::STATE_HOVERED);

  // Remove it.
  model->RemoveItemAt(index);
}

TEST_F(LauncherTest, ShowOverflowBubble) {
  Launcher* launcher = Launcher::ForPrimaryDisplay();
  ASSERT_TRUE(launcher);

  LauncherView* launcher_view = launcher->GetLauncherViewForTest();
  test::LauncherViewTestAPI test(launcher_view);

  LauncherModel* model = launcher_view->model();
  LauncherID first_item_id = model->next_id();

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

  // Removes the first item in main launcher view.
  model->RemoveItemAt(model->ItemIndexByID(first_item_id));

  // Waits for all transitions to finish and there should be no crash.
  test.RunMessageLoopUntilAnimationsDone();
  EXPECT_FALSE(launcher->IsShowingOverflowBubble());
}

// Tests app list button toggles when app list UI shows up.
TEST_F(LauncherTest, AppMenuToggle) {
  AppListButton* app_list_button = static_cast<AppListButton*>(
      Launcher::ForPrimaryDisplay()->GetLauncherViewForTest()->
      GetAppListButtonView());
  Shell* shell = Shell::GetInstance();

  EXPECT_FALSE(shell->GetAppListTargetVisibility());
  EXPECT_FALSE(app_list_button->toggled());
  shell->ToggleAppList(NULL);
  EXPECT_TRUE(shell->GetAppListTargetVisibility());
  EXPECT_TRUE(app_list_button->toggled());
}

// Tests only the app list button with a UI is toggled.
TEST_F(LauncherTest, MultipleAppMenuToggle) {
  UpdateDisplay("300x300,200x200");

  std::vector<AppListButton*> app_list_buttons;
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  for (Shell::RootWindowList::const_iterator it = root_windows.begin();
       it != root_windows.end(); ++it) {
    AppListButton* button = static_cast<AppListButton*>(
        Launcher::ForWindow(*it)->GetLauncherViewForTest()->
        GetAppListButtonView());
    app_list_buttons.push_back(button);
    EXPECT_FALSE(button->toggled());
  }

  Shell* shell = Shell::GetInstance();

  EXPECT_FALSE(shell->GetAppListTargetVisibility());
  shell->ToggleAppList(root_windows[0]);
  EXPECT_TRUE(shell->GetAppListTargetVisibility());
  EXPECT_TRUE(app_list_buttons[0]->toggled());
  EXPECT_FALSE(app_list_buttons[1]->toggled());

  shell->ToggleAppList(NULL);
  EXPECT_FALSE(shell->GetAppListTargetVisibility());

  shell->ToggleAppList(root_windows[1]);
  EXPECT_TRUE(shell->GetAppListTargetVisibility());
  EXPECT_FALSE(app_list_buttons[0]->toggled());
  EXPECT_TRUE(app_list_buttons[1]->toggled());
}

}  // namespace ash
