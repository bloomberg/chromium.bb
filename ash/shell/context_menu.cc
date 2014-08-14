// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell/context_menu.h"

#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_types.h"
#include "ash/shell.h"
#include "grit/ash_strings.h"

namespace ash {
namespace shell {

ContextMenu::ContextMenu(aura::Window* root)
    : ui::SimpleMenuModel(NULL),
      root_window_(root),
      alignment_menu_(root) {
  DCHECK(root_window_);
  set_delegate(this);
  AddCheckItemWithStringId(MENU_AUTO_HIDE,
                           IDS_ASH_SHELF_CONTEXT_MENU_AUTO_HIDE);
  AddSubMenuWithStringId(MENU_ALIGNMENT_MENU,
                         IDS_ASH_SHELF_CONTEXT_MENU_POSITION,
                         &alignment_menu_);
}

ContextMenu::~ContextMenu() {
}

bool ContextMenu::IsCommandIdChecked(int command_id) const {
  switch (command_id) {
    case MENU_AUTO_HIDE:
      return Shell::GetInstance()->GetShelfAutoHideBehavior(root_window_) ==
          ash::SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS;
    default:
      return false;
  }
}

bool ContextMenu::IsCommandIdEnabled(int command_id) const {
  return true;
}

bool ContextMenu::GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) {
  return false;
}

void ContextMenu::ExecuteCommand(int command_id, int event_flags) {
  Shell* shell = Shell::GetInstance();
  switch (static_cast<MenuItem>(command_id)) {
    case MENU_AUTO_HIDE:
      shell->SetShelfAutoHideBehavior(
          shell->GetShelfAutoHideBehavior(root_window_) ==
              SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS ?
                  SHELF_AUTO_HIDE_BEHAVIOR_NEVER :
                  SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS,
          root_window_);
      break;
    case MENU_ALIGNMENT_MENU:
      break;
  }
}

}  // namespace shell
}  // namespace ash
