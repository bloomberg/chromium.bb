// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_COMMON_SHELF_WM_SHELF_H_
#define ASH_WM_COMMON_SHELF_WM_SHELF_H_

#include "ash/ash_export.h"
#include "ash/wm/common/shelf/wm_shelf_types.h"

namespace gfx {
class Rect;
}

namespace ash {
namespace wm {

class WmShelfObserver;
class WmWindow;

// Used for accessing global state.
class ASH_EXPORT WmShelf {
 public:
  // Returns the window showing the shelf.
  virtual WmWindow* GetWindow() = 0;

  virtual ShelfAlignment GetAlignment() const = 0;

  virtual ShelfBackgroundType GetBackgroundType() const = 0;

  virtual void UpdateVisibilityState() = 0;

  virtual ShelfVisibilityState GetVisibilityState() const = 0;

  virtual void UpdateIconPositionForWindow(WmWindow* window) = 0;

  // Returns the screen bounds of the item for the specified window. If there is
  // no item for the specified window an empty rect is returned.
  virtual gfx::Rect GetScreenBoundsOfItemIconForWindow(
      wm::WmWindow* window) = 0;

  virtual void AddObserver(WmShelfObserver* observer) = 0;
  virtual void RemoveObserver(WmShelfObserver* observer) = 0;

 protected:
  virtual ~WmShelf() {}
};

}  // namespace wm
}  // namespace ash

#endif  // ASH_WM_COMMON_SHELF_WM_SHELF_H_
