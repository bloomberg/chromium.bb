// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/compact_layout_manager.h"

#include "ash/wm/window_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace internal {

/////////////////////////////////////////////////////////////////////////////
// CompactLayoutManager, public:

CompactLayoutManager::CompactLayoutManager()
    : status_area_widget_(NULL) {
}

CompactLayoutManager::~CompactLayoutManager() {
}

/////////////////////////////////////////////////////////////////////////////
// CompactLayoutManager, LayoutManager overrides:

void CompactLayoutManager::OnWindowAddedToLayout(aura::Window* child) {
  BaseLayoutManager::OnWindowAddedToLayout(child);
  UpdateStatusAreaVisibility();
}

void CompactLayoutManager::OnWillRemoveWindowFromLayout(aura::Window* child) {
  BaseLayoutManager::OnWillRemoveWindowFromLayout(child);
  UpdateStatusAreaVisibility();
}

void CompactLayoutManager::OnChildWindowVisibilityChanged(aura::Window* child,
                                                          bool visible) {
  BaseLayoutManager::OnChildWindowVisibilityChanged(child, visible);
  UpdateStatusAreaVisibility();
}

/////////////////////////////////////////////////////////////////////////////
// CompactLayoutManager, WindowObserver overrides:

void CompactLayoutManager::OnWindowPropertyChanged(aura::Window* window,
                                                   const char* name,
                                                   void* old) {
  BaseLayoutManager::OnWindowPropertyChanged(window, name, old);
  if (name == aura::client::kShowStateKey)
    UpdateStatusAreaVisibility();
}

//////////////////////////////////////////////////////////////////////////////
// CompactLayoutManager, private:

void CompactLayoutManager::UpdateStatusAreaVisibility() {
  if (!status_area_widget_)
    return;
  // Full screen windows should hide the status area widget.
  if (window_util::HasFullscreenWindow(windows()))
    status_area_widget_->Hide();
  else
    status_area_widget_->Show();
}

}  // namespace internal
}  // namespace ash
