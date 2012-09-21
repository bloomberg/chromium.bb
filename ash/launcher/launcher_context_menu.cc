// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/launcher_context_menu.h"

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/wm/shelf_types.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

LauncherContextMenu::LauncherContextMenu() : ui::SimpleMenuModel(NULL) {
  set_delegate(this);
  AddCheckItemWithStringId(MENU_AUTO_HIDE, GetAutoHideResourceStringId());
  AddSubMenuWithStringId(MENU_ALIGNMENT_MENU,
                         IDS_AURA_LAUNCHER_CONTEXT_MENU_POSITION,
                         &alignment_menu_);
}

LauncherContextMenu::~LauncherContextMenu() {
}

// static
bool LauncherContextMenu::IsAutoHideMenuHideChecked() {
  return ash::SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS ==
      Shell::GetInstance()->GetShelfAutoHideBehavior();
}

// static
ShelfAutoHideBehavior LauncherContextMenu::GetToggledAutoHideBehavior() {
  return IsAutoHideMenuHideChecked() ? ash::SHELF_AUTO_HIDE_BEHAVIOR_NEVER :
      ash::SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS;
}

// static
int LauncherContextMenu::GetAutoHideResourceStringId() {
  return IDS_AURA_LAUNCHER_CONTEXT_MENU_AUTO_HIDE;
}

bool LauncherContextMenu::IsCommandIdChecked(int command_id) const {
  switch (command_id) {
    case MENU_AUTO_HIDE:
      return IsAutoHideMenuHideChecked();
    default:
      return false;
  }
}

bool LauncherContextMenu::IsCommandIdEnabled(int command_id) const {
  return true;
}

bool LauncherContextMenu::GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) {
  return false;
}

void LauncherContextMenu::ExecuteCommand(int command_id) {
  switch (static_cast<MenuItem>(command_id)) {
    case MENU_AUTO_HIDE:
      ash::Shell::GetInstance()->SetShelfAutoHideBehavior(
          GetToggledAutoHideBehavior());
      break;
    case MENU_ALIGNMENT_MENU:
      break;
  }
}

}  // namespace ash
