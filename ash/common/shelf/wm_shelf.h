// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SHELF_WM_SHELF_H_
#define ASH_COMMON_SHELF_WM_SHELF_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/common/shelf/shelf_layout_manager_observer.h"
#include "ash/common/shelf/shelf_types.h"
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
class ShelfWidget;
class StatusAreaWidget;
class WmDimmerView;
class WmShelfObserver;
class WmWindow;

// Controller for the shelf state. Exists for the lifetime of each root window
// controller. Note that the shelf widget may not be created until after login.
class ASH_EXPORT WmShelf : public ShelfLayoutManagerObserver {
 public:
  // Returns the shelf for the display that |window| is on. Note that the shelf
  // widget may not exist, or the shelf may not be visible.
  static WmShelf* ForWindow(WmWindow* window);

  void SetShelf(Shelf* shelf);
  void ClearShelf();
  Shelf* shelf() const { return shelf_; }

  virtual void SetShelfLayoutManager(ShelfLayoutManager* manager);
  ShelfLayoutManager* shelf_layout_manager() const {
    return shelf_layout_manager_;
  }

  // Returns the window showing the shelf.
  WmWindow* GetWindow();

  ShelfAlignment alignment() const { return alignment_; }
  // TODO(jamescook): Replace with alignment().
  ShelfAlignment GetAlignment() const { return alignment_; }
  void SetAlignment(ShelfAlignment alignment);

  // Returns true if the shelf alignment is horizontal (i.e. at the bottom).
  bool IsHorizontalAlignment() const;

  // Returns a value based on shelf alignment.
  int SelectValueForShelfAlignment(int bottom, int left, int right) const;

  // Returns |horizontal| is shelf is horizontal, otherwise |vertical|.
  int PrimaryAxisValue(int horizontal, int vertical) const;

  ShelfAutoHideBehavior auto_hide_behavior() const {
    return auto_hide_behavior_;
  }
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

  // Whether the shelf view is visible.
  // TODO(jamescook): Consolidate this with GetVisibilityState().
  bool IsVisible() const;

  void UpdateVisibilityState();

  ShelfVisibilityState GetVisibilityState() const;

  // Returns the ideal bounds of the shelf assuming it is visible.
  gfx::Rect GetIdealBounds();

  gfx::Rect GetUserWorkAreaBounds() const;

  // Updates the icon position given the current window bounds. This is used
  // when dragging panels to reposition them with respect to the other panels.
  void UpdateIconPositionForPanel(WmWindow* window);

  // Returns the screen bounds of the item for the specified window. If there is
  // no item for the specified window an empty rect is returned.
  gfx::Rect GetScreenBoundsOfItemIconForWindow(WmWindow* window);

  // Launch a 0-indexed shelf item in the shelf. A negative index launches the
  // last shelf item in the shelf.
  static void LaunchShelfItem(int item_index);

  // Activates the shelf item specified by the index in the list of shelf items.
  static void ActivateShelfItem(int item_index);

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
  ShelfWidget* GetShelfWidgetForTesting();

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

  ShelfAlignment alignment_ = SHELF_ALIGNMENT_BOTTOM_LOCKED;

  // Sets shelf alignment to bottom during login and screen lock.
  std::unique_ptr<ShelfLockingManager> shelf_locking_manager_;

  ShelfAutoHideBehavior auto_hide_behavior_ = SHELF_AUTO_HIDE_BEHAVIOR_NEVER;

  base::ObserverList<WmShelfObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(WmShelf);
};

}  // namespace ash

#endif  // ASH_COMMON_SHELF_WM_SHELF_H_
