// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_CONTROLLER_H_
#define ASH_SHELF_SHELF_CONTROLLER_H_

#include "ash/public/cpp/shelf_item.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_model_observer.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/interfaces/shelf.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"

namespace ash {

class Shelf;

// Ash's ShelfController owns the ShelfModel and implements interface functions
// that allow Chrome to modify and observe the Shelf and ShelfModel state.
class ShelfController : public mojom::ShelfController,
                        public ShelfModelObserver {
 public:
  ShelfController();
  ~ShelfController() override;

  // Binds the mojom::ShelfController interface request to this object.
  void BindRequest(mojom::ShelfControllerRequest request);

  ShelfModel* model() { return &model_; }

  // Functions used to notify mojom::ShelfObserver instances of changes.
  void NotifyShelfInitialized(Shelf* shelf);
  void NotifyShelfAlignmentChanged(Shelf* shelf);
  void NotifyShelfAutoHideBehaviorChanged(Shelf* shelf);

  // mojom::ShelfController:
  void AddObserver(mojom::ShelfObserverAssociatedPtrInfo observer) override;
  void SetAlignment(ShelfAlignment alignment, int64_t display_id) override;
  void SetAutoHideBehavior(ShelfAutoHideBehavior auto_hide,
                           int64_t display_id) override;
  void AddShelfItem(int32_t index, const ShelfItem& item) override;
  void RemoveShelfItem(const ShelfID& id) override;
  void MoveShelfItem(const ShelfID& id, int32_t index) override;
  void UpdateShelfItem(const ShelfItem& item) override;
  void SetShelfItemDelegate(const ShelfID& id,
                            mojom::ShelfItemDelegatePtr delegate) override;

  // ShelfModelObserver:
  void ShelfItemAdded(int index) override;
  void ShelfItemRemoved(int index, const ShelfItem& old_item) override;
  void ShelfItemMoved(int start_index, int target_index) override;
  void ShelfItemChanged(int index, const ShelfItem& old_item) override;
  void ShelfItemDelegateChanged(const ShelfID& id,
                                ShelfItemDelegate* delegate) override;

 private:
  // The shelf model shared by all shelf instances.
  ShelfModel model_;

  // Bindings for the ShelfController interface.
  mojo::BindingSet<mojom::ShelfController> bindings_;

  // True when applying changes from the remote ShelfModel owned by Chrome.
  // Changes to the local ShelfModel should not be reported during this time.
  bool applying_remote_shelf_model_changes_ = false;

  // The set of shelf observers notified about state and model changes.
  mojo::AssociatedInterfacePtrSet<mojom::ShelfObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(ShelfController);
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_CONTROLLER_H_
