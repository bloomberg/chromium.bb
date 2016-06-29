// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SHELF_WM_SHELF_H_
#define ASH_COMMON_SHELF_WM_SHELF_H_

#include "ash/ash_export.h"
#include "ash/common/shelf/shelf_types.h"

namespace gfx {
class Rect;
}

namespace ui {
class GestureEvent;
class MouseEvent;
}

namespace ash {

class WmShelfObserver;
class WmWindow;

// Used for accessing global state.
class ASH_EXPORT WmShelf {
 public:
  // Returns the window showing the shelf.
  virtual WmWindow* GetWindow() = 0;

  virtual ShelfAlignment GetAlignment() const = 0;
  virtual void SetAlignment(ShelfAlignment alignment) = 0;

  virtual ShelfAutoHideBehavior GetAutoHideBehavior() const = 0;
  virtual void SetAutoHideBehavior(ShelfAutoHideBehavior behavior) = 0;

  virtual ShelfAutoHideState GetAutoHideState() const = 0;

  // Invoke when the auto-hide state may have changed (for example, when the
  // system tray bubble opens it should force the shelf to be visible).
  virtual void UpdateAutoHideState() = 0;

  virtual ShelfBackgroundType GetBackgroundType() const = 0;

  // Shelf items are slightly dimmed (e.g. when a window is maximized).
  virtual bool IsDimmed() const = 0;

  // Whether the shelf view is visible.
  // TODO(jamescook): Consolidate this with GetVisibilityState().
  virtual bool IsVisible() const = 0;

  virtual void UpdateVisibilityState() = 0;

  virtual ShelfVisibilityState GetVisibilityState() const = 0;

  // Returns the ideal bounds of the shelf assuming it is visible.
  virtual gfx::Rect GetIdealBounds() = 0;

  virtual gfx::Rect GetUserWorkAreaBounds() const = 0;

  virtual void UpdateIconPositionForWindow(WmWindow* window) = 0;

  // Returns the screen bounds of the item for the specified window. If there is
  // no item for the specified window an empty rect is returned.
  virtual gfx::Rect GetScreenBoundsOfItemIconForWindow(WmWindow* window) = 0;

  // Handles a gesture |event| coming from a |target_window| outside the shelf
  // (e.g. the status area widget). Allows support for behaviors like toggling
  // auto-hide with an edge swipe, even if that gesture event hits another
  // window. Returns true if the event was handled.
  virtual bool ProcessGestureEvent(const ui::GestureEvent& event,
                                   WmWindow* target_window) = 0;

  // TODO(jamescook): Nuke when ash_sysui is removed. http://crbug.com/621112
  virtual void UpdateAutoHideForMouseEvent(ui::MouseEvent* event) = 0;
  virtual void UpdateAutoHideForGestureEvent(ui::GestureEvent* event) = 0;

  virtual void AddObserver(WmShelfObserver* observer) = 0;
  virtual void RemoveObserver(WmShelfObserver* observer) = 0;

  // Simulates a virtual keyboard bounds update.
  virtual void SetKeyboardBoundsForTesting(const gfx::Rect& bounds) = 0;

 protected:
  virtual ~WmShelf() {}
};

}  // namespace ash

#endif  // ASH_COMMON_SHELF_WM_SHELF_H_
