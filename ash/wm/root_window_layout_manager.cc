// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/root_window_layout_manager.h"

#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"

namespace aura_shell {
namespace internal {

////////////////////////////////////////////////////////////////////////////////
// RootWindowLayoutManager, public:

RootWindowLayoutManager::RootWindowLayoutManager(aura::Window* owner)
    : owner_(owner),
      background_widget_(NULL) {
}

RootWindowLayoutManager::~RootWindowLayoutManager() {
}

////////////////////////////////////////////////////////////////////////////////
// RootWindowLayoutManager, aura::LayoutManager implementation:

void RootWindowLayoutManager::OnWindowResized() {
  gfx::Rect fullscreen_bounds =
      gfx::Rect(owner_->bounds().width(), owner_->bounds().height());

  aura::Window::Windows::const_iterator i;
  for (i = owner_->children().begin(); i != owner_->children().end(); ++i)
    (*i)->SetBounds(fullscreen_bounds);

  if (background_widget_)
    background_widget_->SetBounds(fullscreen_bounds);
}

void RootWindowLayoutManager::OnWindowAddedToLayout(aura::Window* child) {
}

void RootWindowLayoutManager::OnWillRemoveWindowFromLayout(
    aura::Window* child) {
}

void RootWindowLayoutManager::OnChildWindowVisibilityChanged(
    aura::Window* child,
    bool visible) {
}

void RootWindowLayoutManager::SetChildBounds(
    aura::Window* child,
    const gfx::Rect& requested_bounds) {
  SetChildBoundsDirect(child, requested_bounds);
}

}  // namespace internal
}  // namespace aura_shell
