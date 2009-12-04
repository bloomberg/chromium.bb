// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/menus/menu_model.h"

namespace menus {

bool MenuModel::GetModelAndIndexForCommandId(int command_id,
                                             MenuModel** model, int* index) {
  int item_count = (*model)->GetItemCount();
  for (int i = 0; i < item_count; ++i) {
    if ((*model)->GetTypeAt(i) == TYPE_SUBMENU) {
      MenuModel* submenu_model = (*model)->GetSubmenuModelAt(i);
      if (GetModelAndIndexForCommandId(command_id, &submenu_model, index)) {
        *model = submenu_model;
        return true;
      }
    }
    if ((*model)->GetCommandIdAt(i) == command_id) {
      *index = i;
      return true;
    }
  }
  return false;
}

}  // namespace
