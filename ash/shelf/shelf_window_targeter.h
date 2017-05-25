// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_WINDOW_TARGETER_H_
#define ASH_SHELF_SHELF_WINDOW_TARGETER_H_

#include "ash/shelf/shelf_observer.h"
#include "base/macros.h"
#include "ui/aura/window_observer.h"
#include "ui/wm/core/easy_resize_window_targeter.h"

namespace ash {

class Shelf;
class WmWindow;

// ShelfWindowTargeter makes it easier to resize windows with the mouse when the
// window-edge slightly overlaps with the shelf edge. The targeter also makes it
// easier to drag the shelf out with touch while it is hidden.
class ShelfWindowTargeter : public ::wm::EasyResizeWindowTargeter,
                            public aura::WindowObserver,
                            public ShelfObserver {
 public:
  ShelfWindowTargeter(WmWindow* container, Shelf* shelf);
  ~ShelfWindowTargeter() override;

 private:
  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  // ShelfObserver:
  void WillChangeVisibilityState(ShelfVisibilityState new_state) override;

  Shelf* shelf_;

  DISALLOW_COPY_AND_ASSIGN(ShelfWindowTargeter);
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_WINDOW_TARGETER_H_
