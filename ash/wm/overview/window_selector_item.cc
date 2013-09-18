// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/window_selector_item.h"

namespace ash {

WindowSelectorItem::WindowSelectorItem() {
}

WindowSelectorItem::~WindowSelectorItem() {
}

void WindowSelectorItem::SetBounds(aura::RootWindow* root_window,
                                   const gfx::Rect& target_bounds) {
  bounds_ = target_bounds;
  SetItemBounds(root_window, target_bounds);
}

}  // namespace ash
