// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_ITEM_DELEGATE_MANAGER_H_
#define ASH_SHELF_SHELF_ITEM_DELEGATE_MANAGER_H_

#include <map>

#include "ash/ash_export.h"
#include "ash/launcher/launcher_types.h"
#include "ash/shelf/shelf_model_observer.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"

namespace ash {
class ShelfItemDelegate;
class ShelfModel;

namespace test {
class ShelfItemDelegateManagerTestAPI;
}

// ShelfItemDelegateManager manages the set of ShelfItemDelegates for the
// launcher. ShelfItemDelegateManager does not create ShelfItemDelegates,
// rather it is expected that someone else invokes SetShelfItemDelegate
// appropriately. On the other hand, ShelfItemDelegateManager destroys
// ShelfItemDelegates when the corresponding item from the model is removed.
class ASH_EXPORT ShelfItemDelegateManager : public ShelfModelObserver {
 public:
  explicit ShelfItemDelegateManager(ShelfModel* model);
  virtual ~ShelfItemDelegateManager();

  // Set |item_delegate| for |id| and take an ownership.
  void SetShelfItemDelegate(LauncherID id,
                            scoped_ptr<ShelfItemDelegate> item_delegate);

  // Returns ShelfItemDelegate for |item_type|. Always returns non-NULL.
  ShelfItemDelegate* GetShelfItemDelegate(LauncherID id);

  // ShelfModelObserver overrides:
  virtual void ShelfItemAdded(int model_index) OVERRIDE;
  virtual void ShelfItemRemoved(int index, LauncherID id) OVERRIDE;
  virtual void ShelfItemMoved(int start_index, int targetindex) OVERRIDE;
  virtual void ShelfItemChanged(int index,
                                const LauncherItem& old_item) OVERRIDE;
  virtual void ShelfStatusChanged() OVERRIDE;

 private:
  friend class test::ShelfItemDelegateManagerTestAPI;

  typedef std::map<LauncherID, ShelfItemDelegate*> LauncherIDToItemDelegateMap;

  // Remove and destroy ShelfItemDelegate for |id|.
  void RemoveShelfItemDelegate(LauncherID id);

  // Owned by Shell.
  ShelfModel* model_;

  LauncherIDToItemDelegateMap id_to_item_delegate_map_;

  DISALLOW_COPY_AND_ASSIGN(ShelfItemDelegateManager);
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_ITEM_DELEGATE_MANAGER_H_
