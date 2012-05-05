// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/launcher_context_menu.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/property_util.h"
#include "ash/wm/window_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/layer.h"

namespace ash {

typedef test::AshTestBase LauncherContextMenuTest;

// Various assertions around IsAutoHideMenuHideChecked() and
// ToggleAutoHideMenu().
TEST_F(LauncherContextMenuTest, ToggleAutoHide) {
  scoped_ptr<aura::Window> window(new aura::Window(NULL));
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  window->SetType(aura::client::WINDOW_TYPE_NORMAL);
  window->Init(ui::LAYER_TEXTURED);
  window->SetParent(NULL);
  window->Show();

  Shell* shell = Shell::GetInstance();
  // If the auto-hide behavior isn't DEFAULT, the rest of the tests don't make
  // sense.
  EXPECT_EQ(ash::SHELF_AUTO_HIDE_BEHAVIOR_DEFAULT,
            shell->GetShelfAutoHideBehavior());
  EXPECT_FALSE(LauncherContextMenu::IsAutoHideMenuHideChecked());
  shell->SetShelfAutoHideBehavior(
      LauncherContextMenu::GetToggledAutoHideBehavior());
  EXPECT_EQ(ash::SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS,
            shell->GetShelfAutoHideBehavior());
  shell->SetShelfAutoHideBehavior(
      LauncherContextMenu::GetToggledAutoHideBehavior());
  EXPECT_EQ(ash::SHELF_AUTO_HIDE_BEHAVIOR_DEFAULT,
            shell->GetShelfAutoHideBehavior());

  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  EXPECT_TRUE(LauncherContextMenu::IsAutoHideMenuHideChecked());
  shell->SetShelfAutoHideBehavior(
      LauncherContextMenu::GetToggledAutoHideBehavior());
  EXPECT_EQ(ash::SHELF_AUTO_HIDE_BEHAVIOR_NEVER,
            shell->GetShelfAutoHideBehavior());
  shell->SetShelfAutoHideBehavior(
      LauncherContextMenu::GetToggledAutoHideBehavior());
  EXPECT_EQ(ash::SHELF_AUTO_HIDE_BEHAVIOR_DEFAULT,
            shell->GetShelfAutoHideBehavior());
}

}  // namespace ash
