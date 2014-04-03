// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_alignment_menu.h"

#include "ash/metrics/user_metrics_recorder.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_types.h"
#include "ash/shell.h"
#include "grit/ash_strings.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

ShelfAlignmentMenu::ShelfAlignmentMenu(aura::Window* root)
    : ui::SimpleMenuModel(NULL),
      root_window_(root) {
  DCHECK(root_window_);
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
  return ShelfLayoutManager::ForShelf(root_window_)
      ->SelectValueForShelfAlignment(MENU_ALIGN_BOTTOM == command_id,
                                     MENU_ALIGN_LEFT == command_id,
                                     MENU_ALIGN_RIGHT == command_id,
                                     false);
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
  switch (static_cast<MenuItem>(command_id)) {
    case MENU_ALIGN_LEFT:
      Shell::GetInstance()->metrics()->
          RecordUserMetricsAction(UMA_SHELF_ALIGNMENT_SET_LEFT);
      Shell::GetInstance()->SetShelfAlignment(SHELF_ALIGNMENT_LEFT,
                                              root_window_);
      break;
    case MENU_ALIGN_BOTTOM:
      Shell::GetInstance()->metrics()->
          RecordUserMetricsAction(UMA_SHELF_ALIGNMENT_SET_BOTTOM);
      Shell::GetInstance()->SetShelfAlignment(SHELF_ALIGNMENT_BOTTOM,
                                              root_window_);
      break;
    case MENU_ALIGN_RIGHT:
      Shell::GetInstance()->metrics()->
          RecordUserMetricsAction(UMA_SHELF_ALIGNMENT_SET_RIGHT);
      Shell::GetInstance()->SetShelfAlignment(SHELF_ALIGNMENT_RIGHT,
                                              root_window_);
      break;
  }
}

}  // namespace ash
