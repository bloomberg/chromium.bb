// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/launcher_alignment_menu.h"

#include "ash/shelf_types.h"
#include "ash/shell.h"
#include "ash/wm/shelf_layout_manager.h"
#include "grit/ash_strings.h"
#include "ui/aura/root_window.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

LauncherAlignmentMenu::LauncherAlignmentMenu(
    aura::RootWindow* root)
    : ui::SimpleMenuModel(NULL),
      root_window_(root) {
  DCHECK(root_window_);
  int align_group_id = 1;
  set_delegate(this);
  AddRadioItemWithStringId(MENU_ALIGN_LEFT,
                           IDS_AURA_LAUNCHER_CONTEXT_MENU_ALIGN_LEFT,
                           align_group_id);
  AddRadioItemWithStringId(MENU_ALIGN_BOTTOM,
                           IDS_AURA_LAUNCHER_CONTEXT_MENU_ALIGN_BOTTOM,
                           align_group_id);
  AddRadioItemWithStringId(MENU_ALIGN_RIGHT,
                           IDS_AURA_LAUNCHER_CONTEXT_MENU_ALIGN_RIGHT,
                           align_group_id);
  AddRadioItemWithStringId(MENU_ALIGN_TOP,
                           IDS_AURA_LAUNCHER_CONTEXT_MENU_ALIGN_TOP,
                           align_group_id);
}

LauncherAlignmentMenu::~LauncherAlignmentMenu() {
}

bool LauncherAlignmentMenu::IsCommandIdChecked(int command_id) const {
  return internal::ShelfLayoutManager::ForLauncher(root_window_)->
      SelectValueForShelfAlignment(
          MENU_ALIGN_BOTTOM == command_id,
          MENU_ALIGN_LEFT == command_id,
          MENU_ALIGN_RIGHT == command_id,
          MENU_ALIGN_TOP == command_id);
}

bool LauncherAlignmentMenu::IsCommandIdEnabled(int command_id) const {
  return true;
}

bool LauncherAlignmentMenu::GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) {
  return false;
}

void LauncherAlignmentMenu::ExecuteCommand(int command_id) {
  switch (static_cast<MenuItem>(command_id)) {
    case MENU_ALIGN_LEFT:
      Shell::GetInstance()->SetShelfAlignment(SHELF_ALIGNMENT_LEFT,
                                              root_window_);
      break;
    case MENU_ALIGN_BOTTOM:
      Shell::GetInstance()->SetShelfAlignment(SHELF_ALIGNMENT_BOTTOM,
                                              root_window_);
      break;
    case MENU_ALIGN_RIGHT:
      Shell::GetInstance()->SetShelfAlignment(SHELF_ALIGNMENT_RIGHT,
                                              root_window_);
      break;
    case MENU_ALIGN_TOP:
      Shell::GetInstance()->SetShelfAlignment(SHELF_ALIGNMENT_TOP,
                                              root_window_);
      break;
  }
}

}  // namespace ash
