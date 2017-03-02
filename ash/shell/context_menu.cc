// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell/context_menu.h"

#include "ash/common/shelf/wm_shelf.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/strings/grit/ash_strings.h"

namespace ash {
namespace shell {

ContextMenu::ContextMenu(WmShelf* wm_shelf)
    : ui::SimpleMenuModel(nullptr),
      wm_shelf_(wm_shelf),
      alignment_menu_(wm_shelf) {
  DCHECK(wm_shelf_);
  set_delegate(this);
  AddCheckItemWithStringId(MENU_AUTO_HIDE,
                           IDS_ASH_SHELF_CONTEXT_MENU_AUTO_HIDE);
  AddSubMenuWithStringId(MENU_ALIGNMENT_MENU,
                         IDS_ASH_SHELF_CONTEXT_MENU_POSITION, &alignment_menu_);
}

ContextMenu::~ContextMenu() {}

bool ContextMenu::IsCommandIdChecked(int command_id) const {
  if (command_id == MENU_AUTO_HIDE)
    return wm_shelf_->auto_hide_behavior() == SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS;
  return false;
}

bool ContextMenu::IsCommandIdEnabled(int command_id) const {
  return true;
}

void ContextMenu::ExecuteCommand(int command_id, int event_flags) {
  if (command_id == MENU_AUTO_HIDE) {
    wm_shelf_->SetAutoHideBehavior(wm_shelf_->auto_hide_behavior() ==
                                           SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS
                                       ? SHELF_AUTO_HIDE_BEHAVIOR_NEVER
                                       : SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  }
}

}  // namespace shell
}  // namespace ash
