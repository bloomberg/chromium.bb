// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_COMMON_DOCK_DOCKED_WINDOW_LAYOUT_MANAGER_OBSERVER_H_
#define ASH_WM_COMMON_DOCK_DOCKED_WINDOW_LAYOUT_MANAGER_OBSERVER_H_

#include "ash/wm/common/ash_wm_common_export.h"

namespace gfx {
class Rect;
}

namespace ash {

// Observers to the DockedWindowLayoutManager are notified of significant
// events that occur with the docked windows, such as the bounds change.
class ASH_WM_COMMON_EXPORT DockedWindowLayoutManagerObserver {
 public:
  // Reason for notification. Allows selectively ignoring notifications to
  // prevent a notification loop.
  enum Reason {
    CHILD_CHANGED,
    DISPLAY_RESIZED,
    DISPLAY_INSETS_CHANGED,
    SHELF_ALIGNMENT_CHANGED,
    KEYBOARD_BOUNDS_CHANGING
  };
  // Called after the dock bounds are changed.
  virtual void OnDockBoundsChanging(const gfx::Rect& new_bounds,
                                    Reason reason) = 0;

 protected:
  virtual ~DockedWindowLayoutManagerObserver() {}
};

}  // namespace ash

#endif  // ASH_WM_COMMON_DOCK_DOCKED_WINDOW_LAYOUT_MANAGER_OBSERVER_H_
