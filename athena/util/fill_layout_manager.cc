// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/util/fill_layout_manager.h"

#include "base/logging.h"
#include "ui/aura/window.h"

namespace athena {

FillLayoutManager::FillLayoutManager(aura::Window* container)
    : container_(container) {
  DCHECK(container_);
}

FillLayoutManager::~FillLayoutManager() {
}

void FillLayoutManager::OnWindowResized() {
  gfx::Rect full_bounds = gfx::Rect(container_->bounds().size());
  for (aura::Window::Windows::const_iterator iter =
           container_->children().begin();
       iter != container_->children().end();
       ++iter) {
    SetChildBoundsDirect(*iter, full_bounds);
  }
}

void FillLayoutManager::OnWindowAddedToLayout(aura::Window* child) {
  SetChildBoundsDirect(child, (gfx::Rect(container_->bounds().size())));
}

void FillLayoutManager::OnWillRemoveWindowFromLayout(aura::Window* child) {
}
void FillLayoutManager::OnWindowRemovedFromLayout(aura::Window* child) {
}
void FillLayoutManager::OnChildWindowVisibilityChanged(aura::Window* child,
                                                       bool visible) {
}
void FillLayoutManager::SetChildBounds(aura::Window* child,
                                       const gfx::Rect& requested_bounds) {
  // Ignore SetBounds request.
}

}  // namespace athena
