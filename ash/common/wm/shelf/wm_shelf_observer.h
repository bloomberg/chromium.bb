// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_WM_SHELF_WM_SHELF_OBSERVER_H_
#define ASH_COMMON_WM_SHELF_WM_SHELF_OBSERVER_H_

#include "ash/ash_export.h"
#include "ash/common/wm/background_animator.h"
#include "ash/common/wm/shelf/wm_shelf_types.h"

namespace ash {

class WmWindow;

namespace wm {

// Used to observe changes to the shelf.
class ASH_EXPORT WmShelfObserver {
 public:
  virtual void OnBackgroundUpdated(ShelfBackgroundType background_type,
                                   BackgroundAnimatorChangeType change_type) {}
  virtual void WillChangeVisibilityState(ShelfVisibilityState new_state) {}
  virtual void OnShelfIconPositionsChanged() {}

 protected:
  virtual ~WmShelfObserver() {}
};

}  // namespace wm
}  // namespace ash

#endif  // ASH_COMMON_WM_SHELF_WM_SHELF_OBSERVER_H_
