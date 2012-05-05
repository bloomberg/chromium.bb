// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/root_window_layout_manager.h"

#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace internal {

////////////////////////////////////////////////////////////////////////////////
// RootWindowLayoutManager, public:

RootWindowLayoutManager::RootWindowLayoutManager(aura::Window* owner)
    : owner_(owner),
      background_widget_(NULL) {
}

RootWindowLayoutManager::~RootWindowLayoutManager() {
}

void RootWindowLayoutManager::SetBackgroundWidget(views::Widget* widget) {
  if (widget == background_widget_)
    return;
  // Close now so that the focus manager will be deleted before shutdown.
  if (background_widget_)
    background_widget_->CloseNow();
  background_widget_ = widget;
}

void RootWindowLayoutManager::SetBackgroundLayer(ui::Layer* layer) {
  background_layer_.reset(layer);
}

////////////////////////////////////////////////////////////////////////////////
// RootWindowLayoutManager, aura::LayoutManager implementation:

void RootWindowLayoutManager::OnWindowResized() {
  gfx::Rect fullscreen_bounds =
      gfx::Rect(owner_->bounds().width(), owner_->bounds().height());

  // Resize both our immediate children (the containers-of-containers animated
  // by PowerButtonController) and their children (the actual containers).
  aura::Window::Windows::const_iterator i;
  for (i = owner_->children().begin(); i != owner_->children().end(); ++i) {
    (*i)->SetBounds(fullscreen_bounds);
    aura::Window::Windows::const_iterator j;
    for (j = (*i)->children().begin(); j != (*i)->children().end(); ++j)
      (*j)->SetBounds(fullscreen_bounds);
  }

  if (background_widget_)
    background_widget_->SetBounds(fullscreen_bounds);
  if (background_layer_.get())
    background_layer_->SetBounds(fullscreen_bounds);
}

void RootWindowLayoutManager::OnWindowAddedToLayout(aura::Window* child) {
}

void RootWindowLayoutManager::OnWillRemoveWindowFromLayout(
    aura::Window* child) {
}

void RootWindowLayoutManager::OnWindowRemovedFromLayout(aura::Window* child) {
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
}  // namespace ash
