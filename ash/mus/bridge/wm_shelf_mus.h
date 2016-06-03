// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_BRIDGE_WM_SHELF_MUS_H_
#define ASH_MUS_BRIDGE_WM_SHELF_MUS_H_

#include <stdint.h>

#include <vector>

#include "ash/common/wm/shelf/wm_shelf.h"
#include "base/macros.h"
#include "base/observer_list.h"

namespace ash {
namespace mus {

class ShelfLayoutManager;

// WmShelf implementation for mus.
class WmShelfMus : public wm::WmShelf {
 public:
  explicit WmShelfMus(ShelfLayoutManager* shelf_layout_manager);
  ~WmShelfMus() override;

  // wm::WmShelf:
  wm::WmWindow* GetWindow() override;
  wm::ShelfAlignment GetAlignment() const override;
  wm::ShelfBackgroundType GetBackgroundType() const override;
  void UpdateVisibilityState() override;
  ShelfVisibilityState GetVisibilityState() const override;
  void UpdateIconPositionForWindow(wm::WmWindow* window) override;
  gfx::Rect GetScreenBoundsOfItemIconForWindow(wm::WmWindow* window) override;
  void AddObserver(wm::WmShelfObserver* observer) override;
  void RemoveObserver(wm::WmShelfObserver* observer) override;

 private:
  base::ObserverList<wm::WmShelfObserver> observers_;

  ShelfLayoutManager* shelf_layout_manager_;

  DISALLOW_COPY_AND_ASSIGN(WmShelfMus);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_BRIDGE_WM_SHELF_MUS_H_
