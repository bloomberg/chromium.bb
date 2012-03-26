// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ash/launcher/shelf_auto_hide_menu.h"

#include "ash/shell.h"
#include "chrome/browser/ui/views/ash/launcher/chrome_launcher_delegate.h"
#include "grit/generated_resources.h"

ShelfAutoHideMenu::ShelfAutoHideMenu(ChromeLauncherDelegate* delegate)
    : ui::SimpleMenuModel(NULL),
      delegate_(delegate) {
  set_delegate(this);

  AddRadioItemWithStringId(MENU_AUTO_HIDE_DEFAULT,
                           IDS_LAUNCHER_CONTEXT_MENU_AUTO_HIDE_WHEN_MAXIMIZED,
                           1);
  AddRadioItemWithStringId(MENU_AUTO_HIDE_ALWAYS,
                           IDS_LAUNCHER_CONTEXT_MENU_AUTO_HIDE_SHELF,
                           1);
  AddRadioItemWithStringId(MENU_AUTO_HIDE_NEVER,
                           IDS_LAUNCHER_CONTEXT_MENU_NEVER_AUTO_HIDE,
                           1);
}

ShelfAutoHideMenu::~ShelfAutoHideMenu() {
}

bool ShelfAutoHideMenu::IsCommandIdChecked(int command_id) const {
  ash::ShelfAutoHideBehavior behavior =
      ash::Shell::GetInstance()->GetShelfAutoHideBehavior();
  switch (command_id) {
    case MENU_AUTO_HIDE_DEFAULT:
      return behavior == ash::SHELF_AUTO_HIDE_BEHAVIOR_DEFAULT;
    case MENU_AUTO_HIDE_ALWAYS:
      return behavior == ash::SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS;
    case MENU_AUTO_HIDE_NEVER:
      return behavior == ash::SHELF_AUTO_HIDE_BEHAVIOR_NEVER;
    default:
      return false;
  }
}

bool ShelfAutoHideMenu::IsCommandIdEnabled(int command_id) const {
  return true;
}

bool ShelfAutoHideMenu::GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) {
  return false;
}

void ShelfAutoHideMenu::ExecuteCommand(int command_id) {
  switch (static_cast<MenuItem>(command_id)) {
    case MENU_AUTO_HIDE_DEFAULT:
      delegate_->SetAutoHideBehavior(ash::SHELF_AUTO_HIDE_BEHAVIOR_DEFAULT);
      break;
    case MENU_AUTO_HIDE_ALWAYS:
      delegate_->SetAutoHideBehavior(ash::SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
      break;
    case MENU_AUTO_HIDE_NEVER:
      delegate_->SetAutoHideBehavior(ash::SHELF_AUTO_HIDE_BEHAVIOR_NEVER);
      break;
  }
}
