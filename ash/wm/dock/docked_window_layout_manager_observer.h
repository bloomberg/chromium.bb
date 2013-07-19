// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DOCK_DOCKED_WINDOW_LAYOUT_MANAGER_OBSERVER_H_
#define UI_DOCK_DOCKED_WINDOW_LAYOUT_MANAGER_OBSERVER_H_

#include "ash/ash_export.h"

namespace gfx {
class Rect;
}

namespace ash {
namespace internal {

// Observers to the DockedWindowLayoutManager are notified of significant
// events that occur with the docked windows, such as the bounds change.
class ASH_EXPORT DockedWindowLayoutManagerObserver {
 public:
  // Called after the dock bounds are changed.
  virtual void OnDockBoundsChanging(const gfx::Rect& new_bounds) = 0;

 protected:
  virtual ~DockedWindowLayoutManagerObserver() {}
};

}  // namespace internal
}  // namespace ash

#endif  // UI_DOCK_DOCKED_WINDOW_LAYOUT_MANAGER_OBSERVER_H_
