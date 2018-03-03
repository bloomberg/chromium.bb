// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/chromevox_layout_manager.h"

#include "base/logging.h"

namespace ash {

ChromeVoxLayoutManager::ChromeVoxLayoutManager() = default;

ChromeVoxLayoutManager::~ChromeVoxLayoutManager() = default;

void ChromeVoxLayoutManager::OnWindowAddedToLayout(aura::Window* child) {
  chromevox_window_ = child;
  UpdateLayout();
}

void ChromeVoxLayoutManager::OnWindowRemovedFromLayout(aura::Window* child) {
  // NOTE: In browser_tests a second ChromeVoxPanel can be created while the
  // first one in closing due to races between loading the extension and
  // closing the widget. We only track the latest panel.
  if (child == chromevox_window_)
    chromevox_window_ = nullptr;
  UpdateLayout();
}

void ChromeVoxLayoutManager::SetChildBounds(aura::Window* child,
                                            const gfx::Rect& requested_bounds) {
  SetChildBoundsDirect(child, requested_bounds);
}

void ChromeVoxLayoutManager::UpdateLayout() {
  // TODO: Implement by moving the code out of ChromeVoxPanel.
}

}  // namespace ash
