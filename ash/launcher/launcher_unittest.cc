// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/launcher.h"
#include "ash/launcher/launcher_button.h"
#include "ash/launcher/launcher_model.h"
#include "ash/launcher/launcher_view.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

typedef ash::test::AshTestBase LauncherTest;
using ash::internal::LauncherView;
using ash::internal::LauncherButton;

namespace ash {

// Makes sure invoking SetStatusWidth on the launcher changes the size of the
// LauncherView.
TEST_F(LauncherTest, SetStatusWidth) {
  Launcher* launcher = Shell::GetInstance()->launcher();
  ASSERT_TRUE(launcher);
  views::View* launcher_view = launcher->widget()->GetContentsView();
  ASSERT_EQ(1, launcher_view->child_count());
  launcher_view = launcher_view->child_at(0);

  int total_width = launcher->widget()->GetWindowScreenBounds().width();
  ASSERT_GT(total_width, 0);
  launcher->SetStatusWidth(total_width / 2);
  EXPECT_EQ(total_width - total_width / 2, launcher_view->width());
}

TEST_F(LauncherTest, LaunchApp) {
  Launcher* launcher = Shell::GetInstance()->launcher();
  ASSERT_TRUE(launcher);
  views::View* contents_view = launcher->widget()->GetContentsView();
  ASSERT_EQ(1, contents_view->child_count());
  LauncherView* launcher_view =
      static_cast<LauncherView*>(contents_view->child_at(0));
  LauncherView::TestAPI test(launcher_view);
  LauncherModel* model = launcher->model();

  // Initially we have the app list and chrome icon.
  int button_count = test.GetButtonCount();
  int item_count = model->item_count();

  // Add closed app.
  LauncherItem item(TYPE_APP);
  model->Add(item_count, item);
  ASSERT_EQ(++button_count, test.GetButtonCount());
  LauncherButton* button = test.GetButton(button_count - 1);
  EXPECT_EQ(LauncherButton::STATE_NORMAL, button->state());

  // Open it.
  item = model->items()[item_count];
  item.status = STATUS_RUNNING;
  model->Set(item_count, item);
  EXPECT_EQ(LauncherButton::STATE_RUNNING, button->state());

  // Close it.
  item = model->items()[item_count];
  item.status = STATUS_CLOSED;
  model->Set(item_count, item);
  EXPECT_EQ(LauncherButton::STATE_NORMAL, button->state());

  // Remove it.
  model->RemoveItemAt(item_count);
  ASSERT_EQ(--button_count, test.GetButtonCount());
}

TEST_F(LauncherTest, OpenBrowser) {
  Launcher* launcher = Shell::GetInstance()->launcher();
  ASSERT_TRUE(launcher);
  views::View* contents_view = launcher->widget()->GetContentsView();
  ASSERT_EQ(1, contents_view->child_count());
  LauncherView* launcher_view =
      static_cast<LauncherView*>(contents_view->child_at(0));
  LauncherView::TestAPI test(launcher_view);
  LauncherModel* model = launcher->model();

  // Initially we have the app list and chrome icon.
  int button_count = test.GetButtonCount();
  int item_count = model->item_count();

  // Add running tab.
  LauncherItem item(TYPE_TABBED);
  item.status = STATUS_RUNNING;
  model->Add(item_count, item);
  ASSERT_EQ(++button_count, test.GetButtonCount());
  LauncherButton* button = test.GetButton(button_count - 1);
  EXPECT_EQ(LauncherButton::STATE_RUNNING, button->state());

  // Remove it.
  model->RemoveItemAt(item_count);
  ASSERT_EQ(--button_count, test.GetButtonCount());
}

}  // namespace ash
