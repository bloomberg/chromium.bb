// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_CONTEXT_MENU_MUS_H_
#define ASH_MUS_CONTEXT_MENU_MUS_H_

#include "ash/shelf/shelf_alignment_menu.h"
#include "base/macros.h"
#include "ui/base/models/simple_menu_model.h"

namespace ash {

class Shelf;

// Context menu for mash.
// TODO(msw): Mimic logic in LauncherContextMenu.
class ContextMenuMus : public ui::SimpleMenuModel,
                       public ui::SimpleMenuModel::Delegate {
 public:
  explicit ContextMenuMus(Shelf* shelf);
  ~ContextMenuMus() override;

  // ui::SimpleMenuModel::Delegate overrides:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) override;
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  enum MenuItem {
    MENU_AUTO_HIDE,
    MENU_ALIGNMENT_MENU,
    MENU_CHANGE_WALLPAPER,
  };

  Shelf* shelf_;
  ShelfAlignmentMenu alignment_menu_;

  DISALLOW_COPY_AND_ASSIGN(ContextMenuMus);
};

}  // namespace ash

#endif  // ASH_MUS_CONTEXT_MENU_MUS_H_
