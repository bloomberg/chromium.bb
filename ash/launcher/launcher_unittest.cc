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
  LauncherView* launcher_view = launcher->GetLauncherViewForTest();

  int total_width = launcher->widget()->GetWindowScreenBounds().width();
  ASSERT_GT(total_width, 0);
  launcher->SetStatusWidth(total_width / 2);
  EXPECT_EQ(total_width - total_width / 2, launcher_view->width());
}

// Confirm that launching an app gets the appropriate state reflected in
// its button.
TEST_F(LauncherTest, LaunchApp) {
  Launcher* launcher = Shell::GetInstance()->launcher();
  ASSERT_TRUE(launcher);
  LauncherView* launcher_view = launcher->GetLauncherViewForTest();
  LauncherView::TestAPI test(launcher_view);
  LauncherModel* model = launcher->model();

  // Initially we have the app list and chrome icon.
  int button_count = test.GetButtonCount();
  int item_count = model->item_count();

  // Add closed app.
  LauncherItem item;
  item.type = TYPE_APP;
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

// Confirm that launching a browser gets the appropriate state reflected in
// its button.
TEST_F(LauncherTest, OpenBrowser) {
  Launcher* launcher = Shell::GetInstance()->launcher();
  ASSERT_TRUE(launcher);
  LauncherView* launcher_view = launcher->GetLauncherViewForTest();
  LauncherView::TestAPI test(launcher_view);
  LauncherModel* model = launcher->model();

  // Initially we have the app list and chrome icon.
  int button_count = test.GetButtonCount();
  int item_count = model->item_count();

  // Add running tab.
  LauncherItem item;
  item.type = TYPE_TABBED;
  item.status = STATUS_RUNNING;
  model->Add(item_count, item);
  ASSERT_EQ(++button_count, test.GetButtonCount());
  LauncherButton* button = test.GetButton(button_count - 1);
  EXPECT_EQ(LauncherButton::STATE_RUNNING, button->state());

  // Remove it.
  model->RemoveItemAt(item_count);
  ASSERT_EQ(--button_count, test.GetButtonCount());
}

// Confirm that opening two different browsers and changing their activation
// causes the appropriate state changes in the launcher buttons.
TEST_F(LauncherTest, OpenTwoBrowsers) {
  Launcher* launcher = Shell::GetInstance()->launcher();
  ASSERT_TRUE(launcher);
  LauncherView* launcher_view = launcher->GetLauncherViewForTest();
  LauncherView::TestAPI test(launcher_view);
  LauncherModel* model = launcher->model();

  // Initially we have the app list and chrome icon.
  int button_count = test.GetButtonCount();
  int item_count = model->item_count();
  int button1 = button_count, button2 = button_count + 1;
  int item1 = item_count;

  // Add active tab.
  {
    LauncherItem item;
    item.type = TYPE_TABBED;
    item.status = STATUS_ACTIVE;
    model->Add(item_count, item);
  }
  ASSERT_EQ(++button_count, test.GetButtonCount());
  EXPECT_EQ(LauncherButton::STATE_ACTIVE, test.GetButton(button1)->state());

  // Add new active tab and deactivate other.
  {
    LauncherItem item;
    item.type = TYPE_TABBED;
    model->Add(item_count, item);
    LauncherItem last_item = model->items()[item1];
    last_item.status = STATUS_RUNNING;
    model->Set(item1, last_item);
  }

  ASSERT_EQ(++button_count, test.GetButtonCount());
  EXPECT_EQ(LauncherButton::STATE_RUNNING, test.GetButton(button1)->state());
  EXPECT_EQ(LauncherButton::STATE_ACTIVE, test.GetButton(button2)->state());
}

}  // namespace ash
