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
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

typedef ash::test::AshTestBase LauncherTest;
using ash::internal::LauncherView;
using ash::internal::LauncherButton;

namespace ash {

// Makes sure invoking SetStatusSize on the launcher changes the size of the
// LauncherView.
TEST_F(LauncherTest, SetStatusSize) {
  Launcher* launcher = Shell::GetInstance()->launcher();
  LauncherView* launcher_view = launcher->GetLauncherViewForTest();

  gfx::Size launcher_size = launcher->widget()->GetWindowScreenBounds().size();
  int total_width = launcher_size.width();
  ASSERT_GT(total_width, 0);
  launcher->SetStatusSize(gfx::Size(total_width / 2, launcher_size.height()));
  EXPECT_EQ(total_width - total_width / 2, launcher_view->width());
}

// Confirm that launching a browser gets the appropriate state reflected in
// its button.
TEST_F(LauncherTest, OpenBrowser) {
  Launcher* launcher = Shell::GetInstance()->launcher();
  ASSERT_TRUE(launcher);
  LauncherView* launcher_view = launcher->GetLauncherViewForTest();
  test::LauncherViewTestAPI test(launcher_view);
  LauncherModel* model = launcher->model();

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

}  // namespace ash
