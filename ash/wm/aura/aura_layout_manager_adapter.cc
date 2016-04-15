// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/aura/aura_layout_manager_adapter.h"

#include "ash/wm/aura/wm_window_aura.h"
#include "ash/wm/common/wm_layout_manager.h"

namespace ash {
namespace wm {

AuraLayoutManagerAdapter::AuraLayoutManagerAdapter(
    std::unique_ptr<WmLayoutManager> wm_layout_manager)
    : wm_layout_manager_(std::move(wm_layout_manager)) {}

AuraLayoutManagerAdapter::~AuraLayoutManagerAdapter() {}

void AuraLayoutManagerAdapter::OnWindowResized() {
  wm_layout_manager_->OnWindowResized();
}

void AuraLayoutManagerAdapter::OnWindowAddedToLayout(aura::Window* child) {
  wm_layout_manager_->OnWindowAddedToLayout(WmWindowAura::Get(child));
}

void AuraLayoutManagerAdapter::OnWillRemoveWindowFromLayout(
    aura::Window* child) {
  wm_layout_manager_->OnWillRemoveWindowFromLayout(WmWindowAura::Get(child));
}

void AuraLayoutManagerAdapter::OnWindowRemovedFromLayout(aura::Window* child) {
  wm_layout_manager_->OnWindowRemovedFromLayout(WmWindowAura::Get(child));
}

void AuraLayoutManagerAdapter::OnChildWindowVisibilityChanged(
    aura::Window* child,
    bool visible) {
  wm_layout_manager_->OnChildWindowVisibilityChanged(WmWindowAura::Get(child),
                                                     visible);
}

void AuraLayoutManagerAdapter::SetChildBounds(
    aura::Window* child,
    const gfx::Rect& requested_bounds) {
  wm_layout_manager_->SetChildBounds(WmWindowAura::Get(child),
                                     requested_bounds);
}

}  // namespace wm
}  // namespace ash
