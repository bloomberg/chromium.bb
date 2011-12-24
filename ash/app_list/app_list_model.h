// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APP_LIST_MODEL_H_
#define ASH_APP_LIST_APP_LIST_MODEL_H_
#pragma once

#include "ash/app_list/app_list_item_group_model.h"
#include "base/basictypes.h"
#include "ash/ash_export.h"
#include "ui/base/models/list_model.h"

namespace ash {

// Model for AppListView. It is consisted of a list of AppListItemGroupModels,
// which in turn owns a list of AppListItemModels.
class ASH_EXPORT AppListModel {
 public:
  AppListModel();
  virtual ~AppListModel();

  void AddGroup(AppListItemGroupModel* group);
  AppListItemGroupModel* GetGroup(int index);

  void AddObserver(ui::ListModelObserver* observer);
  void RemoveObserver(ui::ListModelObserver* observer);

  int group_count() const {
    return groups_.item_count();
  }

 private:
  ui::ListModel<AppListItemGroupModel> groups_;

  DISALLOW_COPY_AND_ASSIGN(AppListModel);
};

}  // namespace ash

#endif  // ASH_APP_LIST_APP_LIST_MODEL_H_
