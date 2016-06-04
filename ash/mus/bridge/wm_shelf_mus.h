// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_BRIDGE_WM_SHELF_MUS_H_
#define ASH_MUS_BRIDGE_WM_SHELF_MUS_H_

#include <stdint.h>

#include <vector>

#include "ash/common/shelf/wm_shelf.h"
#include "base/macros.h"
#include "base/observer_list.h"

namespace ash {
namespace mus {

class ShelfLayoutManager;

// WmShelf implementation for mus.
class WmShelfMus : public WmShelf {
 public:
  explicit WmShelfMus(ShelfLayoutManager* shelf_layout_manager);
  ~WmShelfMus() override;

  // WmShelf:
  WmWindow* GetWindow() override;
  ShelfAlignment GetAlignment() const override;
  ShelfBackgroundType GetBackgroundType() const override;
  void UpdateVisibilityState() override;
  ShelfVisibilityState GetVisibilityState() const override;
  void UpdateIconPositionForWindow(WmWindow* window) override;
  gfx::Rect GetScreenBoundsOfItemIconForWindow(WmWindow* window) override;
  void AddObserver(WmShelfObserver* observer) override;
  void RemoveObserver(WmShelfObserver* observer) override;

 private:
  base::ObserverList<WmShelfObserver> observers_;

  ShelfLayoutManager* shelf_layout_manager_;

  DISALLOW_COPY_AND_ASSIGN(WmShelfMus);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_BRIDGE_WM_SHELF_MUS_H_
