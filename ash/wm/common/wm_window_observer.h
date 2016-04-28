// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_COMMON_WM_WINDOW_OBSERVER_H_
#define ASH_WM_COMMON_WM_WINDOW_OBSERVER_H_

#include <stdint.h>

#include "ash/ash_export.h"

namespace gfx {
class Rect;
}

namespace ash {
namespace wm {

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
  virtual void OnWindowTreeChanged(WmWindow* window,
                                   const TreeChangeParams& params) {}

  virtual void OnWindowPropertyChanged(WmWindow* window,
                                       WmWindowProperty property,
                                       intptr_t old) {}

  virtual void OnWindowStackingChanged(WmWindow* window) {}

  virtual void OnWindowDestroying(WmWindow* window) {}

  virtual void OnWindowBoundsChanged(WmWindow* window,
                                     const gfx::Rect& old_bounds,
                                     const gfx::Rect& new_bounds) {}

  virtual void OnWindowVisibilityChanging(wm::WmWindow* window, bool visible) {}

 protected:
  virtual ~WmWindowObserver() {}
};

}  // namespace wm
}  // namespace ash

#endif  // ASH_WM_COMMON_WM_WINDOW_OBSERVER_H_
