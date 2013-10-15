// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LAUNCHER_LAUNCHER_ITEM_DELEGATE_MANAGER_H_
#define ASH_LAUNCHER_LAUNCHER_ITEM_DELEGATE_MANAGER_H_

#include <map>

#include "ash/ash_export.h"
#include "ash/launcher/launcher_model_observer.h"
#include "ash/launcher/launcher_types.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"

namespace ash {
class LauncherItemDelegate;
class LauncherModel;

namespace test {
class LauncherItemDelegateManagerTestAPI;
}

// LauncherItemDelegateManager manages the set of LauncherItemDelegates for the
// launcher. LauncherItemDelegateManager does not create LauncherItemDelegates,
// rather it is expected that someone else invokes SetLauncherItemDelegate
// appropriately. On the other hand, LauncherItemDelegateManager destroys
// LauncherItemDelegates when the corresponding item from the model is removed.
class ASH_EXPORT LauncherItemDelegateManager
    : public ash::LauncherModelObserver {
 public:
  explicit LauncherItemDelegateManager(ash::LauncherModel* model);
  virtual ~LauncherItemDelegateManager();

  // Set |item_delegate| for |id| and take an ownership.
  void SetLauncherItemDelegate(
      ash::LauncherID id,
      scoped_ptr<ash::LauncherItemDelegate> item_delegate);

  // Returns LauncherItemDelegate for |item_type|. Always returns non-NULL.
  LauncherItemDelegate* GetLauncherItemDelegate(ash::LauncherID id);

  // ash::LauncherModelObserver overrides:
  virtual void LauncherItemAdded(int model_index) OVERRIDE;
  virtual void LauncherItemRemoved(int index, ash::LauncherID id) OVERRIDE;
  virtual void LauncherItemMoved(int start_index, int targetindex) OVERRIDE;
  virtual void LauncherItemChanged(
      int index,
      const ash::LauncherItem& old_item) OVERRIDE;
  virtual void LauncherStatusChanged() OVERRIDE;

 private:
  friend class ash::test::LauncherItemDelegateManagerTestAPI;

  typedef std::map<ash::LauncherID, LauncherItemDelegate*>
      LauncherIDToItemDelegateMap;

  // Remove and destroy LauncherItemDelegate for |id|.
  void RemoveLauncherItemDelegate(ash::LauncherID id);

  // Clear all exsiting LauncherItemDelegate for test.
  // Not owned by LauncherItemDelegate.
  ash::LauncherModel* model_;

  LauncherIDToItemDelegateMap id_to_item_delegate_map_;

  DISALLOW_COPY_AND_ASSIGN(LauncherItemDelegateManager);
};

}  // namespace ash

#endif  // ASH_LAUNCHER_LAUNCHER_ITEM_DELEGATE_MANAGER_H_
