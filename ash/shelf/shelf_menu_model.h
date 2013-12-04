// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_MENU_MODEL_H_
#define ASH_SHELF_SHELF_MENU_MODEL_H_

#include "ash/ash_export.h"
#include "ui/base/models/simple_menu_model.h"

namespace ash {

// A special menu model which keeps track of an "active" menu item.
class ASH_EXPORT ShelfMenuModel : public ui::SimpleMenuModel {
 public:
  explicit ShelfMenuModel(ui::SimpleMenuModel::Delegate* delegate)
      : ui::SimpleMenuModel(delegate) {}

  // Returns |true| when the given |command_id| is active and needs to be drawn
  // in a special state.
  virtual bool IsCommandActive(int command_id) const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShelfMenuModel);
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_MENU_MODEL_H_
