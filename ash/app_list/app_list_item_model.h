// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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

namespace aura_shell {

class AppListItemModelObserver;

// AppListItemModel provides icon and title to be shown in a TileView and
// action to be executed when the TileView is activated (clicked or enter
// key it hit).
class ASH_EXPORT AppListItemModel {
 public:
  AppListItemModel();
  virtual ~AppListItemModel();

  // Changes icon and title for the model.
  void SetIcon(const SkBitmap& icon);
  void SetTitle(const std::string& title);

  void AddObserver(AppListItemModelObserver* observer);
  void RemoveObserver(AppListItemModelObserver* observer);

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

}  // namespace aura_shell

#endif  // #define ASH_APP_LIST_APP_LIST_ITEM_MODEL_OBSERVER_H_
