// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_model.h"

namespace aura_shell {

AppListModel::AppListModel() {
}

AppListModel::~AppListModel() {
}

void AppListModel::AddGroup(AppListItemGroupModel* group) {
  groups_.Add(group);
}

AppListItemGroupModel* AppListModel::GetGroup(int index) {
  DCHECK(index >= 0 && index < group_count());
  return groups_.item_at(index);
}

void AppListModel::AddObserver(ui::ListModelObserver* observer) {
  groups_.AddObserver(observer);
}

void AppListModel::RemoveObserver(ui::ListModelObserver* observer) {
  groups_.RemoveObserver(observer);
}

}  // namespace aura_shell
