// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSUI_SHELF_DELEGATE_MUS_H_
#define ASH_SYSUI_SHELF_DELEGATE_MUS_H_

#include <map>

#include "ash/public/interfaces/shelf_layout.mojom.h"
#include "ash/public/interfaces/user_window_controller.mojom.h"
#include "ash/shelf/shelf_delegate.h"
#include "mash/shelf/public/interfaces/shelf.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"

namespace ash {

class ShelfModel;

namespace sysui {

// Manages communication between the ash_sysui shelf, the window manager, and
// the browser.
class ShelfDelegateMus : public ShelfDelegate,
                         public mash::shelf::mojom::ShelfController,
                         public ash::mojom::UserWindowObserver {
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

  // ash::mojom::UserWindowObserver:
  void OnUserWindowObserverAdded(
      mojo::Array<ash::mojom::UserWindowPtr> user_windows) override;
  void OnUserWindowAdded(ash::mojom::UserWindowPtr user_window) override;
  void OnUserWindowRemoved(uint32_t window_id) override;
  void OnUserWindowTitleChanged(uint32_t window_id,
                                const mojo::String& window_title) override;
  void OnUserWindowAppIconChanged(uint32_t window_id,
                                  mojo::Array<uint8_t> app_icon) override;
  void OnUserWindowFocusChanged(uint32_t window_id, bool has_focus) override;

  // Set the Mus window preferred sizes, needed by mash::wm::ShelfLayout.
  void SetShelfPreferredSizes(Shelf* shelf);

  ShelfModel* model_;

  mojo::AssociatedInterfacePtrSet<mash::shelf::mojom::ShelfObserver> observers_;

  ash::mojom::ShelfLayoutPtr shelf_layout_;
  ash::mojom::UserWindowControllerPtr user_window_controller_;
  mojo::Binding<ash::mojom::UserWindowObserver> binding_;
  std::map<uint32_t, ShelfID> window_id_to_shelf_id_;

  std::map<std::string, ShelfID> app_id_to_shelf_id_;
  std::map<ShelfID, std::string> shelf_id_to_app_id_;

  DISALLOW_COPY_AND_ASSIGN(ShelfDelegateMus);
};

}  // namespace sysui
}  // namespace ash

#endif  // ASH_SYSUI_SHELF_DELEGATE_MUS_H_
