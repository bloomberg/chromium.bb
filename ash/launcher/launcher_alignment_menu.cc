// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/launcher_alignment_menu.h"

#include "ash/shell.h"
#include "ash/wm/shelf_auto_hide_behavior.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

LauncherAlignmentMenu::LauncherAlignmentMenu() : ui::SimpleMenuModel(NULL) {
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
}

LauncherAlignmentMenu::~LauncherAlignmentMenu() {
}

bool LauncherAlignmentMenu::IsCommandIdChecked(int command_id) const {
  switch (command_id) {
    case MENU_ALIGN_LEFT:
      return ash::Shell::GetInstance()->GetShelfAlignment() ==
          SHELF_ALIGNMENT_LEFT;
    case MENU_ALIGN_BOTTOM:
      return ash::Shell::GetInstance()->GetShelfAlignment() ==
          SHELF_ALIGNMENT_BOTTOM;
    case MENU_ALIGN_RIGHT:
      return ash::Shell::GetInstance()->GetShelfAlignment() ==
          SHELF_ALIGNMENT_RIGHT;
    default:
      return false;
  }
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
      ash::Shell::GetInstance()->SetShelfAlignment(SHELF_ALIGNMENT_LEFT);
      break;
    case MENU_ALIGN_BOTTOM:
      ash::Shell::GetInstance()->SetShelfAlignment(SHELF_ALIGNMENT_BOTTOM);
      break;
    case MENU_ALIGN_RIGHT:
      ash::Shell::GetInstance()->SetShelfAlignment(SHELF_ALIGNMENT_RIGHT);
      break;
  }
}

}  // namespace ash
