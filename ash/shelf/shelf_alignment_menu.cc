// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_alignment_menu.h"

#include "ash/metrics/user_metrics_recorder.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_types.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "grit/ash_strings.h"
#include "ui/aura/window.h"

namespace ash {

ShelfAlignmentMenu::ShelfAlignmentMenu(Shelf* shelf)
    : ui::SimpleMenuModel(nullptr), shelf_(shelf) {
  DCHECK(shelf_);
  int align_group_id = 1;
  set_delegate(this);
  AddRadioItemWithStringId(MENU_ALIGN_LEFT,
                           IDS_ASH_SHELF_CONTEXT_MENU_ALIGN_LEFT,
                           align_group_id);
  AddRadioItemWithStringId(MENU_ALIGN_BOTTOM,
                           IDS_ASH_SHELF_CONTEXT_MENU_ALIGN_BOTTOM,
                           align_group_id);
  AddRadioItemWithStringId(MENU_ALIGN_RIGHT,
                           IDS_ASH_SHELF_CONTEXT_MENU_ALIGN_RIGHT,
                           align_group_id);
}

ShelfAlignmentMenu::~ShelfAlignmentMenu() {}

bool ShelfAlignmentMenu::IsCommandIdChecked(int command_id) const {
  return shelf_->SelectValueForShelfAlignment(
      MENU_ALIGN_BOTTOM == command_id, MENU_ALIGN_LEFT == command_id,
      MENU_ALIGN_RIGHT == command_id, false);
}

bool ShelfAlignmentMenu::IsCommandIdEnabled(int command_id) const {
  return true;
}

bool ShelfAlignmentMenu::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) {
  return false;
}

void ShelfAlignmentMenu::ExecuteCommand(int command_id, int event_flags) {
  Shell* shell = Shell::GetInstance();
  ShelfLayoutManager* manager = shelf_->shelf_widget()->shelf_layout_manager();
  switch (static_cast<MenuItem>(command_id)) {
    case MENU_ALIGN_LEFT:
      shell->metrics()->RecordUserMetricsAction(UMA_SHELF_ALIGNMENT_SET_LEFT);
      manager->SetAlignment(SHELF_ALIGNMENT_LEFT);
      break;
    case MENU_ALIGN_BOTTOM:
      shell->metrics()->RecordUserMetricsAction(UMA_SHELF_ALIGNMENT_SET_BOTTOM);
      manager->SetAlignment(SHELF_ALIGNMENT_BOTTOM);
      break;
    case MENU_ALIGN_RIGHT:
      shell->metrics()->RecordUserMetricsAction(UMA_SHELF_ALIGNMENT_SET_RIGHT);
      manager->SetAlignment(SHELF_ALIGNMENT_RIGHT);
      break;
  }
}

}  // namespace ash
