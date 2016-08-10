// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSUI_SHELF_DELEGATE_MUS_H_
#define ASH_SYSUI_SHELF_DELEGATE_MUS_H_

#include <map>

#include "ash/common/shelf/shelf_delegate.h"
#include "mash/shelf/public/interfaces/shelf.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"

namespace ash {

class ShelfModel;

namespace sysui {

// Manages communication between the mash shelf and the browser.
// TODO(mash): Support ShelfController in mojo:ash and remove this sysui impl.
class ShelfDelegateMus : public ShelfDelegate,
                         public mash::shelf::mojom::ShelfController {
 public:
  explicit ShelfDelegateMus(ShelfModel* model);
  ~ShelfDelegateMus() override;

 private:
  // ShelfDelegate:
  void OnShelfCreated(Shelf* shelf) override;
  void OnShelfDestroyed(Shelf* shelf) override;
  void OnShelfAlignmentChanged(Shelf* shelf) override;
  void OnShelfAutoHideBehaviorChanged(Shelf* shelf) override;
  void OnShelfAutoHideStateChanged(Shelf* shelf) override;
  void OnShelfVisibilityStateChanged(Shelf* shelf) override;
  ShelfID GetShelfIDForAppID(const std::string& app_id) override;
  bool HasShelfIDToAppIDMapping(ShelfID id) const override;
  const std::string& GetAppIDForShelfID(ShelfID id) override;
  void PinAppWithID(const std::string& app_id) override;
  bool IsAppPinned(const std::string& app_id) override;
  void UnpinAppWithID(const std::string& app_id) override;

  // mash::shelf::mojom::ShelfController:
  void AddObserver(
      mash::shelf::mojom::ShelfObserverAssociatedPtrInfo observer) override;
  void SetAlignment(mash::shelf::mojom::Alignment alignment) override;
  void SetAutoHideBehavior(
      mash::shelf::mojom::AutoHideBehavior auto_hide) override;
  void PinItem(
      mash::shelf::mojom::ShelfItemPtr item,
      mash::shelf::mojom::ShelfItemDelegateAssociatedPtrInfo delegate) override;
  void UnpinItem(const mojo::String& app_id) override;
  void SetItemImage(const mojo::String& app_id, const SkBitmap& image) override;

  // Set the Mus window preferred sizes.
  void SetShelfPreferredSizes(Shelf* shelf);

  ShelfModel* model_;

  mojo::AssociatedInterfacePtrSet<mash::shelf::mojom::ShelfObserver> observers_;

  std::map<uint32_t, ShelfID> window_id_to_shelf_id_;

  std::map<std::string, ShelfID> app_id_to_shelf_id_;
  std::map<ShelfID, std::string> shelf_id_to_app_id_;

  DISALLOW_COPY_AND_ASSIGN(ShelfDelegateMus);
};

}  // namespace sysui
}  // namespace ash

#endif  // ASH_SYSUI_SHELF_DELEGATE_MUS_H_
