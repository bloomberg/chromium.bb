// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APP_LIST_MODEL_H_
#define ASH_APP_LIST_APP_LIST_MODEL_H_
#pragma once

#include "base/basictypes.h"
#include "ash/app_list/app_list_item_model.h"
#include "ash/ash_export.h"
#include "ui/base/models/list_model.h"

namespace ash {

class AppListItemModel;

// Model for AppListModelView that owns a list of AppListItemModels.
class ASH_EXPORT AppListModel {
 public:
  AppListModel();
  virtual ~AppListModel();

  // Adds an item to the model. The model takes ownership of |item|.
  void AddItem(AppListItemModel* item);

  AppListItemModel* GetItem(int index);

  void AddObserver(ui::ListModelObserver* observer);
  void RemoveObserver(ui::ListModelObserver* observer);

  int item_count() const {
    return items_.item_count();
  }

 private:
  typedef ui::ListModel<AppListItemModel> Items;
  Items items_;

  DISALLOW_COPY_AND_ASSIGN(AppListModel);
};

}  // namespace ash

#endif  // ASH_APP_LIST_APP_LIST_MODEL_H_
