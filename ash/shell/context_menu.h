// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_CONTEXT_MENU_H_
#define ASH_SHELL_CONTEXT_MENU_H_

#include "ash/common/shelf/shelf_alignment_menu.h"
#include "base/macros.h"
#include "ui/base/models/simple_menu_model.h"

namespace ash {

class WmShelf;

namespace shell {

// Context menu for the ash shell.
class ContextMenu : public ui::SimpleMenuModel,
                    public ui::SimpleMenuModel::Delegate {
 public:
  explicit ContextMenu(WmShelf* wm_shelf);
  ~ContextMenu() override;

  // ui::SimpleMenuModel::Delegate overrides:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  enum MenuItem {
    MENU_AUTO_HIDE,
    MENU_ALIGNMENT_MENU,
  };

  WmShelf* wm_shelf_;
  ShelfAlignmentMenu alignment_menu_;

  DISALLOW_COPY_AND_ASSIGN(ContextMenu);
};

}  // namespace shell
}  // namespace ash

#endif  // ASH_SHELL_CONTEXT_MENU_H_
