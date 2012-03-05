// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_APP_LIST_APP_LIST_ITEM_MODEL_H_
#define UI_AURA_SHELL_APP_LIST_APP_LIST_ITEM_MODEL_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/observer_list.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ash/ash_export.h"

namespace ui {
class MenuModel;
}

namespace ash {

class AppListItemModelObserver;

// AppListItemModel provides icon and title to be shown in a AppListItemView
// and action to be executed when the AppListItemView is activated.
class ASH_EXPORT AppListItemModel {
 public:
  AppListItemModel();
  virtual ~AppListItemModel();

  // Changes icon and title for the model.
  void SetIcon(const SkBitmap& icon);
  void SetTitle(const std::string& title);

  void AddObserver(AppListItemModelObserver* observer);
  void RemoveObserver(AppListItemModelObserver* observer);

  // Returns the context menu model for this item.
  // Note the menu model is owned by this item.
  virtual ui::MenuModel* GetContextMenuModel();

  const SkBitmap& icon() const {
    return icon_;
  }

  const std::string& title() const {
    return title_;
  }

 private:
  SkBitmap icon_;
  std::string title_;

  ObserverList<AppListItemModelObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(AppListItemModel);
};

}  // namespace ash

#endif  // #define ASH_APP_LIST_APP_LIST_ITEM_MODEL_OBSERVER_H_
