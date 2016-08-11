// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AURA_WM_SHELF_AURA_H_
#define ASH_AURA_WM_SHELF_AURA_H_

#include "ash/ash_export.h"
#include "ash/common/shelf/shelf_icon_observer.h"
#include "ash/common/shelf/wm_shelf.h"
#include "ash/shelf/shelf_layout_manager_observer.h"
#include "base/macros.h"
#include "base/observer_list.h"

namespace ash {

class Shelf;
class ShelfBezelEventHandler;
class ShelfLayoutManager;

// Aura implementation of WmShelf.
class ASH_EXPORT WmShelfAura : public WmShelf,
                               public ShelfLayoutManagerObserver,
                               public ShelfIconObserver {
 public:
  WmShelfAura();
  ~WmShelfAura() override;

  // This object is initialized in multiple steps as the RootWindowController
  // constructs the components of the shelf.
  void SetShelfLayoutManager(ShelfLayoutManager* shelf_layout_manager);
  void SetShelf(Shelf* shelf);

  void Shutdown();

  static Shelf* GetShelf(WmShelf* shelf);

 private:
  class AutoHideEventHandler;

  void ResetShelfLayoutManager();

  // WmShelf:
  WmWindow* GetWindow() override;
  ShelfAlignment GetAlignment() const override;
  void SetAlignment(ShelfAlignment alignment) override;
  ShelfAutoHideBehavior GetAutoHideBehavior() const override;
  void SetAutoHideBehavior(ShelfAutoHideBehavior behavior) override;
  ShelfAutoHideState GetAutoHideState() const override;
  void UpdateAutoHideState() override;
  ShelfBackgroundType GetBackgroundType() const override;
  WmDimmerView* CreateDimmerView(bool disable_animations_for_test) override;
  bool IsDimmed() const override;
  bool IsShowingOverflowBubble() const override;
  void SchedulePaint() override;
  bool IsVisible() const override;
  void UpdateVisibilityState() override;
  ShelfVisibilityState GetVisibilityState() const override;
  gfx::Rect GetIdealBounds() override;
  gfx::Rect GetUserWorkAreaBounds() const override;
  void UpdateIconPositionForWindow(WmWindow* window) override;
  gfx::Rect GetScreenBoundsOfItemIconForWindow(WmWindow* window) override;
  bool ProcessGestureEvent(const ui::GestureEvent& event) override;
  void AddObserver(WmShelfObserver* observer) override;
  void RemoveObserver(WmShelfObserver* observer) override;
  void SetKeyboardBoundsForTesting(const gfx::Rect& bounds) override;
  ShelfLockingManager* GetShelfLockingManagerForTesting() override;
  ShelfView* GetShelfViewForTesting() override;

  // ShelfLayoutManagerObserver:
  void WillDeleteShelfLayoutManager() override;
  void OnAutoHideStateChanged(ShelfAutoHideState new_state) override;
  void OnBackgroundUpdated(ShelfBackgroundType background_type,
                           BackgroundAnimatorChangeType change_type) override;
  void WillChangeVisibilityState(ShelfVisibilityState new_state) override;

  // ShelfIconObserver:
  void OnShelfIconPositionsChanged() override;

  // May be null during login and during initialization of a secondary display.
  Shelf* shelf_ = nullptr;

  // Cached separately because it may be destroyed before |shelf_|.
  ShelfLayoutManager* shelf_layout_manager_ = nullptr;

  base::ObserverList<WmShelfObserver> observers_;

  // Forwards mouse and gesture events to ShelfLayoutManager for auto-hide.
  // TODO(mash): Facilitate simliar functionality in mash: crbug.com/631216
  std::unique_ptr<AutoHideEventHandler> auto_hide_event_handler_;

  // Forwards touch gestures on a bezel sensor to the shelf.
  // TODO(mash): Facilitate simliar functionality in mash: crbug.com/636647
  std::unique_ptr<ShelfBezelEventHandler> bezel_event_handler_;

  DISALLOW_COPY_AND_ASSIGN(WmShelfAura);
};

}  // namespace ash

#endif  // ASH_AURA_WM_SHELF_AURA_H_
