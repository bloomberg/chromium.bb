// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_SHELF_DELEGATE_MUS_H_
#define ASH_MUS_SHELF_DELEGATE_MUS_H_

#include <map>

#include "ash/shelf/shelf_delegate.h"
#include "mash/wm/public/interfaces/user_window_controller.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace ash {

class ShelfModel;

namespace sysui {

class ShelfDelegateMus : public ShelfDelegate,
                         public mash::wm::mojom::UserWindowObserver {
 public:
  explicit ShelfDelegateMus(ShelfModel* model);
  ~ShelfDelegateMus() override;

 private:
  // ShelfDelegate:
  void OnShelfCreated(Shelf* shelf) override;
  void OnShelfDestroyed(Shelf* shelf) override;
  ShelfID GetShelfIDForAppID(const std::string& app_id) override;
  bool HasShelfIDToAppIDMapping(ShelfID id) const override;
  const std::string& GetAppIDForShelfID(ShelfID id) override;
  void PinAppWithID(const std::string& app_id) override;
  bool IsAppPinned(const std::string& app_id) override;
  void UnpinAppWithID(const std::string& app_id) override;

  // mash::wm::mojom::UserWindowObserver:
  void OnUserWindowObserverAdded(
      mojo::Array<mash::wm::mojom::UserWindowPtr> user_windows) override;
  void OnUserWindowAdded(mash::wm::mojom::UserWindowPtr user_window) override;
  void OnUserWindowRemoved(uint32_t window_id) override;
  void OnUserWindowTitleChanged(uint32_t window_id,
                                const mojo::String& window_title) override;
  void OnUserWindowFocusChanged(uint32_t window_id, bool has_focus) override;

  ShelfModel* model_;

  mash::wm::mojom::UserWindowControllerPtr user_window_controller_;
  mojo::Binding<mash::wm::mojom::UserWindowObserver> binding_;
  std::map<uint32_t, ShelfID> window_id_to_shelf_id_;

  DISALLOW_COPY_AND_ASSIGN(ShelfDelegateMus);
};

}  // namespace sysui
}  // namespace ash

#endif  // ASH_MUS_SHELF_DELEGATE_MUS_H_
