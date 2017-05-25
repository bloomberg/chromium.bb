// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_alignment_menu.h"

#include "ash/metrics/user_metrics_action.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/shelf/shelf.h"
#include "ash/shell_port.h"
#include "ash/strings/grit/ash_strings.h"

namespace ash {

ShelfAlignmentMenu::ShelfAlignmentMenu(Shelf* shelf)
    : ui::SimpleMenuModel(nullptr), shelf_(shelf) {
  DCHECK(shelf_);
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
  switch (shelf_->alignment()) {
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
  switch (static_cast<MenuItem>(command_id)) {
    case MENU_ALIGN_LEFT:
      ShellPort::Get()->RecordUserMetricsAction(UMA_SHELF_ALIGNMENT_SET_LEFT);
      shelf_->SetAlignment(SHELF_ALIGNMENT_LEFT);
      break;
    case MENU_ALIGN_BOTTOM:
      ShellPort::Get()->RecordUserMetricsAction(UMA_SHELF_ALIGNMENT_SET_BOTTOM);
      shelf_->SetAlignment(SHELF_ALIGNMENT_BOTTOM);
      break;
    case MENU_ALIGN_RIGHT:
      ShellPort::Get()->RecordUserMetricsAction(UMA_SHELF_ALIGNMENT_SET_RIGHT);
      shelf_->SetAlignment(SHELF_ALIGNMENT_RIGHT);
      break;
  }
}

}  // namespace ash
