// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/menu_container_layout_manager.h"

#include "ash/wm/window_animations.h"
#include "ui/aura/client/window_types.h"
#include "ui/aura/window.h"

namespace ash {
namespace internal {

MenuContainerLayoutManager::MenuContainerLayoutManager() {
}

MenuContainerLayoutManager::~MenuContainerLayoutManager() {
}

void MenuContainerLayoutManager::OnWindowResized() {
}

void MenuContainerLayoutManager::OnWindowAddedToLayout(aura::Window* child) {
  DCHECK(child->type() == aura::client::WINDOW_TYPE_MENU ||
         child->type() == aura::client::WINDOW_TYPE_TOOLTIP);
}

void MenuContainerLayoutManager::OnWillRemoveWindowFromLayout(
    aura::Window* child) {
}

void MenuContainerLayoutManager::OnChildWindowVisibilityChanged(
    aura::Window* child,
    bool visible) {
  if (child->type() == aura::client::WINDOW_TYPE_TOOLTIP)
    AnimateOnChildWindowVisibilityChanged(child, visible);
}

void MenuContainerLayoutManager::SetChildBounds(
    aura::Window* child,
    const gfx::Rect& requested_bounds) {
  SetChildBoundsDirect(child, requested_bounds);
}

}  // namespace internal
}  // namespace ash
