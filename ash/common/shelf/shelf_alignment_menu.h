// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SHELF_SHELF_ALIGNMENT_MENU_H_
#define ASH_COMMON_SHELF_SHELF_ALIGNMENT_MENU_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/base/models/simple_menu_model.h"

namespace ash {

class WmShelf;

// Submenu for choosing the alignment of the shelf.
class ASH_EXPORT ShelfAlignmentMenu : public ui::SimpleMenuModel,
                                      public ui::SimpleMenuModel::Delegate {
 public:
  explicit ShelfAlignmentMenu(WmShelf* wm_shelf);
  ~ShelfAlignmentMenu() override;

  // ui::SimpleMenuModel::Delegate overrides:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  enum MenuItem {
    // Offset so as not to interfere with other menus.
    MENU_ALIGN_LEFT = 500,
    MENU_ALIGN_RIGHT,
    MENU_ALIGN_BOTTOM,
  };

  WmShelf* wm_shelf_;

  DISALLOW_COPY_AND_ASSIGN(ShelfAlignmentMenu);
};

}  // namespace ash

#endif  // ASH_COMMON_SHELF_SHELF_ALIGNMENT_MENU_H_
