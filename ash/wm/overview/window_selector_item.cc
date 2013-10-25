// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/window_selector_item.h"
#include "base/auto_reset.h"
#include "ui/aura/window.h"

namespace ash {

WindowSelectorItem::WindowSelectorItem()
    : root_window_(NULL),
      in_bounds_update_(false) {
}

WindowSelectorItem::~WindowSelectorItem() {
}

void WindowSelectorItem::SetBounds(aura::Window* root_window,
                                   const gfx::Rect& target_bounds) {
  if (in_bounds_update_)
    return;
  base::AutoReset<bool> auto_reset_in_bounds_update(&in_bounds_update_, true);
  root_window_ = root_window;
  target_bounds_ = target_bounds;
  SetItemBounds(root_window, target_bounds, true);
}

void WindowSelectorItem::RecomputeWindowTransforms() {
  if (in_bounds_update_ || target_bounds_.IsEmpty())
    return;
  DCHECK(root_window_);
  base::AutoReset<bool> auto_reset_in_bounds_update(&in_bounds_update_, true);
  SetItemBounds(root_window_, target_bounds_, false);
}

}  // namespace ash
