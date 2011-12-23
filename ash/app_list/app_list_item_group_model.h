// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APP_LIST_ITEM_GROUP_MODEL_H_
#define ASH_APP_LIST_APP_LIST_ITEM_GROUP_MODEL_H_
#pragma once

#include <string>

#include "ash/app_list/app_list_item_model.h"
#include "base/basictypes.h"
#include "ui/aura_shell/aura_shell_export.h"
#include "ui/base/models/list_model.h"

namespace aura_shell {

// AppListItemGroupModel holds a list of AppListItemModels.
class AURA_SHELL_EXPORT AppListItemGroupModel {
 public:
  typedef ui::ListModel<AppListItemModel> Items;

  explicit AppListItemGroupModel(const std::string& title);
  virtual ~AppListItemGroupModel();

  void AddItem(AppListItemModel* item);
  AppListItemModel* GetItem(int index);

  const std::string& title() const {
    return title_;
  }

  int item_count() const {
    return items_.item_count();
  }

 private:
  const std::string title_;
  Items items_;

  DISALLOW_COPY_AND_ASSIGN(AppListItemGroupModel);
};

}  // namespace aura_shell

#endif  // ASH_APP_LIST_APP_LIST_ITEM_GROUP_MODEL_H_
