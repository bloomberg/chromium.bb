// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/wm/root_window_layout_manager.h"

#include "ash/common/wm_window.h"
#include "ash/common/wm_window_tracker.h"

namespace ash {
namespace wm {

////////////////////////////////////////////////////////////////////////////////
// RootWindowLayoutManager, public:

RootWindowLayoutManager::RootWindowLayoutManager(WmWindow* owner)
    : owner_(owner) {}

RootWindowLayoutManager::~RootWindowLayoutManager() {}

////////////////////////////////////////////////////////////////////////////////
// RootWindowLayoutManager, aura::LayoutManager implementation:

void RootWindowLayoutManager::OnWindowResized() {
  const gfx::Rect fullscreen_bounds = gfx::Rect(owner_->GetBounds().size());

  // Resize both our immediate children (the containers-of-containers animated
  // by PowerButtonController) and their children (the actual containers).
  WmWindowTracker children_tracker(owner_->GetChildren());
  while (!children_tracker.windows().empty()) {
    WmWindow* child = children_tracker.Pop();
    child->SetBounds(fullscreen_bounds);
    WmWindowTracker grandchildren_tracker(child->GetChildren());
    while (!grandchildren_tracker.windows().empty())
      grandchildren_tracker.Pop()->SetBounds(fullscreen_bounds);
  }
}

void RootWindowLayoutManager::OnWindowAddedToLayout(WmWindow* child) {}

void RootWindowLayoutManager::OnWillRemoveWindowFromLayout(WmWindow* child) {}

void RootWindowLayoutManager::OnWindowRemovedFromLayout(WmWindow* child) {}

void RootWindowLayoutManager::OnChildWindowVisibilityChanged(WmWindow* child,
                                                             bool visible) {}

void RootWindowLayoutManager::SetChildBounds(
    WmWindow* child,
    const gfx::Rect& requested_bounds) {
  child->SetBoundsDirect(requested_bounds);
}

}  // namespace wm
}  // namespace ash
