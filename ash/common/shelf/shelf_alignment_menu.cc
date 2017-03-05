// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/shelf/shelf_alignment_menu.h"

#include "ash/common/metrics/user_metrics_action.h"
#include "ash/common/shelf/wm_shelf.h"
#include "ash/common/wm_shell.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/strings/grit/ash_strings.h"

namespace ash {

ShelfAlignmentMenu::ShelfAlignmentMenu(WmShelf* wm_shelf)
    : ui::SimpleMenuModel(nullptr), wm_shelf_(wm_shelf) {
  DCHECK(wm_shelf_);
  const int align_group_id = 1;
  set_delegate(this);
  AddRadioItemWithStringId(
      MENU_ALIGN_LEFT, IDS_ASH_SHELF_CONTEXT_MENU_ALIGN_LEFT, align_group_id);
  AddRadioItemWithStringId(MENU_ALIGN_BOTTOM,
                           IDS_ASH_SHELF_CONTEXT_MENU_ALIGN_BOTTOM,
                           align_group_id);
  AddRadioItemWithStringId(
      MENU_ALIGN_RIGHT, IDS_ASH_SHELF_CONTEXT_MENU_ALIGN_RIGHT, align_group_id);
}

ShelfAlignmentMenu::~ShelfAlignmentMenu() {}

bool ShelfAlignmentMenu::IsCommandIdChecked(int command_id) const {
  switch (wm_shelf_->GetAlignment()) {
    case SHELF_ALIGNMENT_BOTTOM:
    case SHELF_ALIGNMENT_BOTTOM_LOCKED:
      return command_id == MENU_ALIGN_BOTTOM;
    case SHELF_ALIGNMENT_LEFT:
      return command_id == MENU_ALIGN_LEFT;
    case SHELF_ALIGNMENT_RIGHT:
      return command_id == MENU_ALIGN_RIGHT;
  }
  return false;
}

bool ShelfAlignmentMenu::IsCommandIdEnabled(int command_id) const {
  return true;
}

void ShelfAlignmentMenu::ExecuteCommand(int command_id, int event_flags) {
  WmShell* shell = WmShell::Get();
  switch (static_cast<MenuItem>(command_id)) {
    case MENU_ALIGN_LEFT:
      shell->RecordUserMetricsAction(UMA_SHELF_ALIGNMENT_SET_LEFT);
      wm_shelf_->SetAlignment(SHELF_ALIGNMENT_LEFT);
      break;
    case MENU_ALIGN_BOTTOM:
      shell->RecordUserMetricsAction(UMA_SHELF_ALIGNMENT_SET_BOTTOM);
      wm_shelf_->SetAlignment(SHELF_ALIGNMENT_BOTTOM);
      break;
    case MENU_ALIGN_RIGHT:
      shell->RecordUserMetricsAction(UMA_SHELF_ALIGNMENT_SET_RIGHT);
      wm_shelf_->SetAlignment(SHELF_ALIGNMENT_RIGHT);
      break;
  }
}

}  // namespace ash
