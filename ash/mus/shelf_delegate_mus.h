// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_SHELF_DELEGATE_MUS_H_
#define ASH_MUS_SHELF_DELEGATE_MUS_H_

#include <map>
#include <string>

#include "ash/common/shelf/shelf_delegate.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/interfaces/shelf.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"

namespace ash {

class ShelfModel;

// Manages communication between the mash shelf and the browser.
class ShelfDelegateMus : public ShelfDelegate, public mojom::ShelfController {
 public:
  explicit ShelfDelegateMus(ShelfModel* model);
  ~ShelfDelegateMus() override;

 private:
  // ShelfDelegate:
  void OnShelfCreated(WmShelf* shelf) override;
  void OnShelfDestroyed(WmShelf* shelf) override;
  void OnShelfAlignmentChanged(WmShelf* shelf) override;
  void OnShelfAutoHideBehaviorChanged(WmShelf* shelf) override;
  void OnShelfAutoHideStateChanged(WmShelf* shelf) override;
  void OnShelfVisibilityStateChanged(WmShelf* shelf) override;
  ShelfID GetShelfIDForAppID(const std::string& app_id) override;
  ShelfID GetShelfIDForAppIDAndLaunchID(const std::string& app_id,
                                        const std::string& launch_id) override;
  bool HasShelfIDToAppIDMapping(ShelfID id) const override;
  const std::string& GetAppIDForShelfID(ShelfID id) override;
  void PinAppWithID(const std::string& app_id) override;
  bool IsAppPinned(const std::string& app_id) override;
  void UnpinAppWithID(const std::string& app_id) override;

  // mojom::ShelfController:
  void AddObserver(mojom::ShelfObserverAssociatedPtrInfo observer) override;
  void SetAlignment(ShelfAlignment alignment, int64_t display_id) override;
  void SetAutoHideBehavior(ShelfAutoHideBehavior auto_hide,
                           int64_t display_id) override;
  void PinItem(mojom::ShelfItemPtr item,
               mojom::ShelfItemDelegateAssociatedPtrInfo delegate) override;
  void UnpinItem(const std::string& app_id) override;
  void SetItemImage(const std::string& app_id, const SkBitmap& image) override;

  ShelfModel* model_;

  mojo::AssociatedInterfacePtrSet<mojom::ShelfObserver> observers_;

  std::map<uint32_t, ShelfID> window_id_to_shelf_id_;

  std::map<std::string, ShelfID> app_id_to_shelf_id_;
  std::map<ShelfID, std::string> shelf_id_to_app_id_;

  DISALLOW_COPY_AND_ASSIGN(ShelfDelegateMus);
};

}  // namespace ash

#endif  // ASH_MUS_SHELF_DELEGATE_MUS_H_
