// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_AURA_WM_SHELF_AURA_H_
#define ASH_WM_AURA_WM_SHELF_AURA_H_

#include "ash/ash_export.h"
#include "ash/shelf/shelf_icon_observer.h"
#include "ash/shelf/shelf_layout_manager_observer.h"
#include "ash/wm/common/shelf/wm_shelf.h"
#include "base/macros.h"
#include "base/observer_list.h"

namespace ash {

class Shelf;
class ShelfLayoutManager;

namespace wm {

// Aura implementation of WmShelf.
class ASH_EXPORT WmShelfAura : public WmShelf,
                               public ShelfLayoutManagerObserver,
                               public ShelfIconObserver {
 public:
  explicit WmShelfAura(Shelf* shelf);
  ~WmShelfAura() override;

  static Shelf* GetShelf(WmShelf* shelf);

 private:
  // WmShelf:
  WmWindow* GetWindow() override;
  ShelfAlignment GetAlignment() const override;
  ShelfBackgroundType GetBackgroundType() const override;
  void UpdateVisibilityState() override;
  ShelfVisibilityState GetVisibilityState() const override;
  void UpdateIconPositionForWindow(WmWindow* window) override;
  gfx::Rect GetScreenBoundsOfItemIconForWindow(wm::WmWindow* window) override;
  void AddObserver(WmShelfObserver* observer) override;
  void RemoveObserver(WmShelfObserver* observer) override;

  // ShelfLayoutManagerObserver:
  void WillDeleteShelf() override;
  void OnBackgroundUpdated(wm::ShelfBackgroundType background_type,
                           BackgroundAnimatorChangeType change_type) override;
  void WillChangeVisibilityState(ShelfVisibilityState new_state) override;

  // ShelfIconObserver:
  void OnShelfIconPositionsChanged() override;

  Shelf* shelf_;
  base::ObserverList<WmShelfObserver> observers_;
  // ShelfLayoutManager is cached separately as it may be destroyed before
  // WmShelfAura.
  ShelfLayoutManager* shelf_layout_manager_;

  DISALLOW_COPY_AND_ASSIGN(WmShelfAura);
};

}  // namespace wm
}  // namespace ash

#endif  // ASH_WM_AURA_WM_SHELF_AURA_H_
