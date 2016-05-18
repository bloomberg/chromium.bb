// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_WM_BRIDGE_WM_SHELF_MUS_H_
#define MASH_WM_BRIDGE_WM_SHELF_MUS_H_

#include <stdint.h>

#include <vector>

#include "ash/wm/common/shelf/wm_shelf.h"
#include "base/macros.h"
#include "base/observer_list.h"

namespace mash {
namespace wm {

class ShelfLayoutManager;

// WmShelf implementation for mus.
class WmShelfMus : public ash::wm::WmShelf {
 public:
  explicit WmShelfMus(ShelfLayoutManager* shelf_layout_manager);
  ~WmShelfMus() override;

  // ash::wm::WmShelf:
  ash::wm::WmWindow* GetWindow() override;
  ash::wm::ShelfAlignment GetAlignment() const override;
  ash::wm::ShelfBackgroundType GetBackgroundType() const override;
  void UpdateVisibilityState() override;
  ash::ShelfVisibilityState GetVisibilityState() const override;
  void UpdateIconPositionForWindow(ash::wm::WmWindow* window) override;
  gfx::Rect GetScreenBoundsOfItemIconForWindow(
      ash::wm::WmWindow* window) override;
  void AddObserver(ash::wm::WmShelfObserver* observer) override;
  void RemoveObserver(ash::wm::WmShelfObserver* observer) override;

 private:
  base::ObserverList<ash::wm::WmShelfObserver> observers_;

  ShelfLayoutManager* shelf_layout_manager_;

  DISALLOW_COPY_AND_ASSIGN(WmShelfMus);
};

}  // namespace wm
}  // namespace mash

#endif  // MASH_WM_BRIDGE_WM_SHELF_MUS_H_
