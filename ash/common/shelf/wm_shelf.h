// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SHELF_WM_SHELF_H_
#define ASH_COMMON_SHELF_WM_SHELF_H_

#include "ash/ash_export.h"
#include "ash/common/shelf/shelf_types.h"
#include "ash/shelf/shelf_layout_manager_observer.h"
#include "base/observer_list.h"

namespace gfx {
class Rect;
}

namespace ui {
class GestureEvent;
}

namespace ash {

class Shelf;
class ShelfLayoutManager;
class ShelfLockingManager;
class ShelfView;
class StatusAreaWidget;
class WmDimmerView;
class WmShelfObserver;
class WmWindow;

// Used for accessing global state.
class ASH_EXPORT WmShelf : public ShelfLayoutManagerObserver {
 public:
  void SetShelf(Shelf* shelf);
  void ClearShelf();
  Shelf* shelf() const { return shelf_; }

  virtual void SetShelfLayoutManager(ShelfLayoutManager* manager);
  ShelfLayoutManager* shelf_layout_manager() const {
    return shelf_layout_manager_;
  }

  // Returns the window showing the shelf.
  WmWindow* GetWindow();

  ShelfAlignment GetAlignment() const;
  void SetAlignment(ShelfAlignment alignment);

  // Returns true if the shelf alignment is horizontal (i.e. at the bottom).
  bool IsHorizontalAlignment() const;

  // Returns a value based on shelf alignment.
  int SelectValueForShelfAlignment(int bottom, int left, int right) const;

  // Returns |horizontal| is shelf is horizontal, otherwise |vertical|.
  int PrimaryAxisValue(int horizontal, int vertical) const;

  ShelfAutoHideBehavior GetAutoHideBehavior() const;
  void SetAutoHideBehavior(ShelfAutoHideBehavior behavior);

  ShelfAutoHideState GetAutoHideState() const;

  // Invoke when the auto-hide state may have changed (for example, when the
  // system tray bubble opens it should force the shelf to be visible).
  void UpdateAutoHideState();

  ShelfBackgroundType GetBackgroundType() const;

  // Creates a view that dims shelf items. The returned view is owned by its
  // widget. Returns null if shelf dimming is not supported (e.g. on mus).
  // TODO(jamescook): Delete this after material design ships, as MD will not
  // require shelf dimming. http://crbug.com/614453
  virtual WmDimmerView* CreateDimmerView(bool disable_animations_for_test);

  // Shelf items are slightly dimmed (e.g. when a window is maximized).
  // TODO(jamescook): Delete this after material design ships, as MD will not
  // require shelf dimming. http://crbug.com/614453
  bool IsDimmed() const;

  // Schedules a repaint for all shelf buttons.
  // TODO(jamescook): Eliminate when ShelfView moves to //ash/common.
  // http://crbug.com/615155
  void SchedulePaint();

  // Whether the shelf view is visible.
  // TODO(jamescook): Consolidate this with GetVisibilityState().
  bool IsVisible() const;

  void UpdateVisibilityState();

  ShelfVisibilityState GetVisibilityState() const;

  // Returns the ideal bounds of the shelf assuming it is visible.
  gfx::Rect GetIdealBounds();

  gfx::Rect GetUserWorkAreaBounds() const;

  void UpdateIconPositionForWindow(WmWindow* window);

  // Returns the screen bounds of the item for the specified window. If there is
  // no item for the specified window an empty rect is returned.
  gfx::Rect GetScreenBoundsOfItemIconForWindow(WmWindow* window);

  // Handles a gesture |event| coming from a source outside the shelf widget
  // (e.g. the status area widget). Allows support for behaviors like toggling
  // auto-hide with a swipe, even if that gesture event hits another window.
  // Returns true if the event was handled.
  bool ProcessGestureEvent(const ui::GestureEvent& event);

  void AddObserver(WmShelfObserver* observer);
  void RemoveObserver(WmShelfObserver* observer);

  void NotifyShelfIconPositionsChanged();
  StatusAreaWidget* GetStatusAreaWidget() const;

  void SetVirtualKeyboardBoundsForTesting(const gfx::Rect& bounds);
  ShelfLockingManager* GetShelfLockingManagerForTesting();
  ShelfView* GetShelfViewForTesting();

 protected:
  WmShelf();
  ~WmShelf() override;

  // ShelfLayoutManagerObserver:
  void WillDeleteShelfLayoutManager() override;
  void WillChangeVisibilityState(ShelfVisibilityState new_state) override;
  void OnAutoHideStateChanged(ShelfAutoHideState new_state) override;
  void OnBackgroundUpdated(ShelfBackgroundType background_type,
                           BackgroundAnimatorChangeType change_type) override;

 private:
  // Legacy shelf controller. Null before login and in secondary display init.
  // Instance lifetimes are managed by ash::RootWindowController and WmShelfMus.
  Shelf* shelf_ = nullptr;

  // Layout manager for the shelf container window. Instances are constructed by
  // ShelfWidget and lifetimes are managed by the container windows themselves.
  ShelfLayoutManager* shelf_layout_manager_ = nullptr;

  base::ObserverList<WmShelfObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(WmShelf);
};

}  // namespace ash

#endif  // ASH_COMMON_SHELF_WM_SHELF_H_
