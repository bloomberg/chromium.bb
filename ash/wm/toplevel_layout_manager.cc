// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/toplevel_layout_manager.h"

#include "ash/wm/shelf_layout_manager.h"
#include "ash/wm/window_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/screen.h"

namespace aura_shell {
namespace internal {

ToplevelLayoutManager::ToplevelLayoutManager() : shelf_(NULL) {
}

ToplevelLayoutManager::~ToplevelLayoutManager() {
  for (Windows::const_iterator i = windows_.begin(); i != windows_.end(); ++i)
    (*i)->RemoveObserver(this);
}

void ToplevelLayoutManager::OnWindowResized() {
}

void ToplevelLayoutManager::OnWindowAddedToLayout(aura::Window* child) {
  windows_.insert(child);
  child->AddObserver(this);
  if (child->GetProperty(aura::client::kShowStateKey)) {
    UpdateBoundsFromShowState(child);
    UpdateShelfVisibility();
  }
}

void ToplevelLayoutManager::OnWillRemoveWindowFromLayout(
    aura::Window* child) {
  windows_.erase(child);
  child->RemoveObserver(this);
  UpdateShelfVisibility();
}

void ToplevelLayoutManager::OnChildWindowVisibilityChanged(aura::Window* child,
                                                           bool visibile) {
  UpdateShelfVisibility();
}

void ToplevelLayoutManager::SetChildBounds(aura::Window* child,
                                           const gfx::Rect& requested_bounds) {
  const static int kTitleHeight = 12;
  gfx::Rect child_bounds(requested_bounds);
  gfx::Rect work_area = gfx::Screen::GetMonitorWorkAreaNearestWindow(child);
  if (child_bounds.y() < 0)
    child_bounds.set_y(0);
  else if (child_bounds.y() + kTitleHeight > work_area.bottom())
    child_bounds.set_y(work_area.bottom() - kTitleHeight);
  SetChildBoundsDirect(child, child_bounds);
}

void ToplevelLayoutManager::OnWindowPropertyChanged(aura::Window* window,
                                                    const char* name,
                                                    void* old) {
  if (name == aura::client::kShowStateKey) {
    UpdateBoundsFromShowState(window);
    UpdateShelfVisibility();
  }
}

void ToplevelLayoutManager::UpdateShelfVisibility() {
  if (!shelf_)
    return;
  shelf_->SetVisible(!HasFullscreenWindow(windows_));
}

}  // namespace internal
}  // namespace aura_shell
