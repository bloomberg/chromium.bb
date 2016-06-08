// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/sysui/context_menu_mus.h"

#include "ash/common/shelf/shelf_types.h"
#include "ash/common/shelf/wm_shelf.h"
#include "ash/desktop_background/user_wallpaper_delegate.h"
#include "ash/shell.h"
#include "grit/ash_strings.h"

namespace ash {

ContextMenuMus::ContextMenuMus(WmShelf* wm_shelf)
    : ui::SimpleMenuModel(nullptr),
      wm_shelf_(wm_shelf),
      alignment_menu_(wm_shelf) {
  set_delegate(this);
  AddCheckItemWithStringId(MENU_AUTO_HIDE,
                           IDS_ASH_SHELF_CONTEXT_MENU_AUTO_HIDE);
  AddSubMenuWithStringId(MENU_ALIGNMENT_MENU,
                         IDS_ASH_SHELF_CONTEXT_MENU_POSITION, &alignment_menu_);
#if defined(OS_CHROMEOS)
  AddItemWithStringId(MENU_CHANGE_WALLPAPER, IDS_AURA_SET_DESKTOP_WALLPAPER);
#endif
}

ContextMenuMus::~ContextMenuMus() {}

bool ContextMenuMus::IsCommandIdChecked(int command_id) const {
  if (command_id == MENU_AUTO_HIDE)
    return wm_shelf_->GetAutoHideBehavior() == SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS;
  return false;
}

bool ContextMenuMus::IsCommandIdEnabled(int command_id) const {
  Shell* shell = Shell::GetInstance();
  if (command_id == MENU_CHANGE_WALLPAPER)
    return shell->user_wallpaper_delegate()->CanOpenSetWallpaperPage();
  return true;
}

bool ContextMenuMus::GetAcceleratorForCommandId(int command_id,
                                                ui::Accelerator* accelerator) {
  return false;
}

void ContextMenuMus::ExecuteCommand(int command_id, int event_flags) {
  if (command_id == MENU_AUTO_HIDE) {
    wm_shelf_->SetAutoHideBehavior(wm_shelf_->GetAutoHideBehavior() ==
                                           SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS
                                       ? SHELF_AUTO_HIDE_BEHAVIOR_NEVER
                                       : SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  } else if (command_id == MENU_CHANGE_WALLPAPER) {
    Shell::GetInstance()->user_wallpaper_delegate()->OpenSetWallpaperPage();
  }
}

}  // namespace ash
