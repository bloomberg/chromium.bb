// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_CONTROLLER_H_
#define ASH_SHELF_SHELF_CONTROLLER_H_

#include <map>
#include <string>

#include "ash/public/cpp/shelf_item.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/interfaces/shelf.mojom.h"
#include "ash/shelf/shelf_model.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"

namespace ash {

class WmShelf;

// Ash's implementation of the mojom::ShelfController interface. Chrome connects
// to this interface to observe and manage the per-display ash shelf instances.
class ShelfController : public mojom::ShelfController {
 public:
  ShelfController();
  ~ShelfController() override;

  // Binds the mojom::ShelfController interface request to this object.
  void BindRequest(mojom::ShelfControllerRequest request);

  ShelfModel* model() { return &model_; }

  // Functions used to notify mojom::ShelfObserver instances of changes.
  void NotifyShelfCreated(WmShelf* shelf);
  void NotifyShelfAlignmentChanged(WmShelf* shelf);
  void NotifyShelfAutoHideBehaviorChanged(WmShelf* shelf);

  // mojom::Shelf:
  void AddObserver(mojom::ShelfObserverAssociatedPtrInfo observer) override;
  void SetAlignment(ShelfAlignment alignment, int64_t display_id) override;
  void SetAutoHideBehavior(ShelfAutoHideBehavior auto_hide,
                           int64_t display_id) override;
  void PinItem(const ShelfItem& item,
               mojom::ShelfItemDelegateAssociatedPtrInfo delegate) override;
  void UnpinItem(const std::string& app_id) override;
  void SetItemImage(const std::string& app_id, const SkBitmap& image) override;

 private:
  // The shelf model shared by all shelf instances.
  ShelfModel model_;

  // Bindings for the ShelfController interface.
  mojo::BindingSet<mojom::ShelfController> bindings_;

  // The set of shelf observers notified about shelf state and settings changes.
  mojo::AssociatedInterfacePtrSet<mojom::ShelfObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(ShelfController);
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_CONTROLLER_H_
