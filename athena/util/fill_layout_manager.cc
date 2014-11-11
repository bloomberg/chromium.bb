// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/util/fill_layout_manager.h"

#include "base/logging.h"
#include "ui/aura/window.h"
#include "ui/aura/window_property.h"

// This is to avoid creating type definitoin for kAlwaysFillWindowKey.
DECLARE_EXPORTED_WINDOW_PROPERTY_TYPE(ATHENA_EXPORT, bool);

namespace athena {
namespace {

DEFINE_LOCAL_WINDOW_PROPERTY_KEY(bool, kAlwaysFillWindowKey, false);

// TODO(oshima): Implement real window/layout manager. crbug.com/388362.
bool ShouldFill(aura::Window* window) {
  return window->GetProperty(kAlwaysFillWindowKey) ||
         (window->type() != ui::wm::WINDOW_TYPE_MENU &&
          window->type() != ui::wm::WINDOW_TYPE_TOOLTIP &&
          window->type() != ui::wm::WINDOW_TYPE_POPUP);
}

}  // namespace

// static
void FillLayoutManager::SetAlwaysFill(aura::Window* window) {
  window->SetProperty(kAlwaysFillWindowKey, true);
}

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
    if (ShouldFill(*iter))
      SetChildBoundsDirect(*iter, full_bounds);
  }
}

void FillLayoutManager::OnWindowAddedToLayout(aura::Window* child) {
  if (ShouldFill(child))
    SetChildBoundsDirect(child, gfx::Rect(container_->bounds().size()));
}

void FillLayoutManager::OnWillRemoveWindowFromLayout(aura::Window* child) {
}

void FillLayoutManager::OnWindowRemovedFromLayout(aura::Window* child) {
}

void FillLayoutManager::OnChildWindowVisibilityChanged(aura::Window* child,
                                                       bool visible) {
  if (visible && ShouldFill(child))
    SetChildBoundsDirect(child, gfx::Rect(container_->bounds().size()));
}

void FillLayoutManager::SetChildBounds(aura::Window* child,
                                       const gfx::Rect& requested_bounds) {
  if (!ShouldFill(child))
    SetChildBoundsDirect(child, requested_bounds);
}

}  // namespace athena
