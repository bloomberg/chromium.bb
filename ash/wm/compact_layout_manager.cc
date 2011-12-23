// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/compact_layout_manager.h"

#include "ash/wm/window_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/screen.h"
#include "ui/views/widget/widget.h"

namespace aura_shell {
namespace internal {

CompactLayoutManager::CompactLayoutManager(views::Widget* status_area_widget)
    : status_area_widget_(status_area_widget) {
}

CompactLayoutManager::~CompactLayoutManager() {
  for (Windows::const_iterator i = windows_.begin(); i != windows_.end(); ++i)
    (*i)->RemoveObserver(this);
}

void CompactLayoutManager::OnWindowResized() {
}

void CompactLayoutManager::OnWindowAddedToLayout(aura::Window* child) {
  windows_.insert(child);
  child->AddObserver(this);
  if (child->GetProperty(aura::client::kShowStateKey)) {
    UpdateBoundsFromShowState(child);
    UpdateStatusAreaVisibility();
  }
}

void CompactLayoutManager::OnWillRemoveWindowFromLayout(aura::Window* child) {
  windows_.erase(child);
  child->RemoveObserver(this);
}

void CompactLayoutManager::OnChildWindowVisibilityChanged(
    aura::Window* child,
    bool visibile) {
  UpdateStatusAreaVisibility();
}

void CompactLayoutManager::SetChildBounds(
    aura::Window* child,
    const gfx::Rect& requested_bounds) {
  gfx::Rect bounds = requested_bounds;
  // Avoid a janky resize on startup by ensuring the initial bounds fill the
  // screen.
  if (IsWindowMaximized(child))
    bounds = gfx::Screen::GetPrimaryMonitorBounds();
  SetChildBoundsDirect(child, bounds);
}

void CompactLayoutManager::OnWindowPropertyChanged(aura::Window* window,
                                                      const char* name,
                                                      void* old) {
  if (name == aura::client::kShowStateKey) {
    UpdateBoundsFromShowState(window);
    UpdateStatusAreaVisibility();
  }
}

//////////////////////////////////////////////////////////////////////////////
// CompactLayoutManager, private:

void CompactLayoutManager::UpdateStatusAreaVisibility() {
  if (!status_area_widget_)
    return;
  // Full screen windows should hide the status area widget.
  bool fullscreen_window = HasFullscreenWindow(windows_);
  bool widget_visible = status_area_widget_->IsVisible();
  if (fullscreen_window && widget_visible)
    status_area_widget_->Hide();
  else if (!fullscreen_window && !widget_visible)
    status_area_widget_->Show();
}

}  // namespace internal
}  // namespace aura_shell
