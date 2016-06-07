// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_WM_WINDOW_OBSERVER_H_
#define ASH_COMMON_WM_WINDOW_OBSERVER_H_

#include <stdint.h>

#include "ash/ash_export.h"

namespace gfx {
class Rect;
}

namespace ash {

class WmWindow;
enum class WmWindowProperty;

class ASH_EXPORT WmWindowObserver {
 public:
  struct TreeChangeParams {
    WmWindow* target = nullptr;
    WmWindow* old_parent = nullptr;
    WmWindow* new_parent = nullptr;
  };

  // |window| is the window the observer was added to, which is not necessarily
  // the window that was added/removed.
  virtual void OnWindowTreeChanging(WmWindow* window,
                                    const TreeChangeParams& params) {}
  virtual void OnWindowTreeChanged(WmWindow* window,
                                   const TreeChangeParams& params) {}

  virtual void OnWindowPropertyChanged(WmWindow* window,
                                       WmWindowProperty property) {}

  virtual void OnWindowStackingChanged(WmWindow* window) {}

  virtual void OnWindowDestroying(WmWindow* window) {}
  virtual void OnWindowDestroyed(WmWindow* window) {}

  virtual void OnWindowBoundsChanged(WmWindow* window,
                                     const gfx::Rect& old_bounds,
                                     const gfx::Rect& new_bounds) {}

  virtual void OnWindowVisibilityChanging(WmWindow* window, bool visible) {}

  virtual void OnWindowTitleChanged(WmWindow* window) {}

 protected:
  virtual ~WmWindowObserver() {}
};

}  // namespace ash

#endif  // ASH_COMMON_WM_WINDOW_OBSERVER_H_
