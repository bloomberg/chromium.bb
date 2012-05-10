// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/launcher_context_menu.h"

#include "ash/shell.h"
#include "ash/wm/shelf_auto_hide_behavior.h"
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
  ash::Shell* shell = ash::Shell::GetInstance();
  ash::ShelfAutoHideBehavior auto_hide_behavior =
      shell->GetShelfAutoHideBehavior();
  return (shell->IsInMaximizedMode() &&
          (auto_hide_behavior == ash::SHELF_AUTO_HIDE_BEHAVIOR_DEFAULT ||
           auto_hide_behavior == ash::SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS)) ||
      (!shell->IsInMaximizedMode() &&
       auto_hide_behavior == ash::SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
}

// static
ShelfAutoHideBehavior LauncherContextMenu::GetToggledAutoHideBehavior() {
  ash::Shell* shell = ash::Shell::GetInstance();
  ash::ShelfAutoHideBehavior auto_hide_behavior;
  if (shell->IsInMaximizedMode()) {
    if (IsAutoHideMenuHideChecked())
      auto_hide_behavior = ash::SHELF_AUTO_HIDE_BEHAVIOR_NEVER;
    else
      auto_hide_behavior = ash::SHELF_AUTO_HIDE_BEHAVIOR_DEFAULT;
  } else if (IsAutoHideMenuHideChecked()) {
    auto_hide_behavior = ash::SHELF_AUTO_HIDE_BEHAVIOR_DEFAULT;
  } else {
    auto_hide_behavior = ash::SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS;
  }
  return auto_hide_behavior;
}

// static
int LauncherContextMenu::GetAutoHideResourceStringId() {
  return ash::Shell::GetInstance()->IsInMaximizedMode() ?
      IDS_AURA_LAUNCHER_CONTEXT_MENU_AUTO_HIDE_MAXIMIZED :
      IDS_AURA_LAUNCHER_CONTEXT_MENU_AUTO_HIDE_NOT_MAXIMIZED;
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
