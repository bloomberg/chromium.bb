// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell/context_menu.h"

#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_types.h"
#include "grit/ash_strings.h"

namespace ash {
namespace shell {

ContextMenu::ContextMenu(ash::Shelf* shelf)
    : ui::SimpleMenuModel(nullptr), shelf_(shelf), alignment_menu_(shelf) {
  DCHECK(shelf_);
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
  if (command_id == MENU_AUTO_HIDE)
    return shelf_->GetAutoHideBehavior() == SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS;
  return false;
}

bool ContextMenu::IsCommandIdEnabled(int command_id) const {
  return true;
}

bool ContextMenu::GetAcceleratorForCommandId(int command_id,
                                             ui::Accelerator* accelerator) {
  return false;
}

void ContextMenu::ExecuteCommand(int command_id, int event_flags) {
  if (command_id == MENU_AUTO_HIDE) {
    shelf_->SetAutoHideBehavior(shelf_->GetAutoHideBehavior() ==
                                        SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS
                                    ? SHELF_AUTO_HIDE_BEHAVIOR_NEVER
                                    : SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  }
}

}  // namespace shell
}  // namespace ash
