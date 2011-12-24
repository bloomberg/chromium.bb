// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_item_group_model.h"

namespace ash {

AppListItemGroupModel::AppListItemGroupModel(const std::string& title)
    : title_(title) {
}

AppListItemGroupModel::~AppListItemGroupModel() {
}

void AppListItemGroupModel::AddItem(AppListItemModel* item) {
  items_.Add(item);
}

AppListItemModel* AppListItemGroupModel::GetItem(int index) {
  DCHECK(index >= 0 && index < item_count());
  return items_.item_at(index);
}

}  // namespace ash
