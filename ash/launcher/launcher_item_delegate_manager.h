// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LAUNCHER_LAUNCHER_ITEM_DELEGATE_MANAGER_H_
#define ASH_LAUNCHER_LAUNCHER_ITEM_DELEGATE_MANAGER_H_

#include <map>

#include "ash/ash_export.h"
#include "ash/launcher/launcher_types.h"
#include "base/compiler_specific.h"

namespace ash {
class LauncherItemDelegate;

// LauncherItemDelegateManager helps Launcher/LauncherView to choose right
// LauncherItemDelegate based on LauncherItemType.
// When new LauncherItemDelegate is created, it must be registered by
// RegisterLauncherItemDelegate(). If not, Launcher/LauncherView can't get
// LauncherItem's LauncherItemDelegate.
// TODO(simon.hong81): This class should own all LauncherItemDelegate.
class ASH_EXPORT LauncherItemDelegateManager {
 public:
  LauncherItemDelegateManager();
  virtual ~LauncherItemDelegateManager();

  // Returns LauncherItemDelegate for |item_type|.
  // This class doesn't own each LauncherItemDelegate for now.
  LauncherItemDelegate* GetLauncherItemDelegate(
      ash::LauncherItemType item_type);

  // Register |item_delegate| for |type|.
  // For now, This class doesn't own |item_delegate|.
  // TODO(simon.hong81): Register LauncherItemDelegate with LauncherID.
  void RegisterLauncherItemDelegate(
      ash::LauncherItemType type, LauncherItemDelegate* item_delegate);

 private:
  typedef std::map<ash::LauncherItemType, LauncherItemDelegate*>
      LauncherItemTypeToItemDelegateMap;

  LauncherItemTypeToItemDelegateMap item_type_to_item_delegate_map_;

  DISALLOW_COPY_AND_ASSIGN(LauncherItemDelegateManager);
};

}  // namespace ash

#endif  // ASH_LAUNCHER_LAUNCHER_ITEM_DELEGATE_MANAGER_H_
