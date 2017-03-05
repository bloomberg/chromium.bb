// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/aura/aura_layout_manager_adapter.h"

#include "ash/common/wm_layout_manager.h"
#include "ash/common/wm_window.h"
#include "ui/aura/window.h"
#include "ui/base/class_property.h"

DECLARE_UI_CLASS_PROPERTY_TYPE(ash::AuraLayoutManagerAdapter*);

namespace ash {
namespace {
// AuraLayoutManagerAdapter is an aura::LayoutManager, so it's owned by the
// aura::Window it is installed on. This property is used to lookup the
// AuraLayoutManagerAdapter given only an aura::Window.
DEFINE_UI_CLASS_PROPERTY_KEY(AuraLayoutManagerAdapter*,
                             kAuraLayoutManagerAdapter,
                             nullptr);
}  // namespace

AuraLayoutManagerAdapter::AuraLayoutManagerAdapter(
    aura::Window* window,
    std::unique_ptr<WmLayoutManager> wm_layout_manager)
    : window_(window), wm_layout_manager_(std::move(wm_layout_manager)) {
  window->SetProperty(kAuraLayoutManagerAdapter, this);
}

AuraLayoutManagerAdapter::~AuraLayoutManagerAdapter() {
  // Only one AuraLayoutManagerAdapter is created per window at a time, so this
  // AuraLayoutManagerAdapter should be the installed AuraLayoutManagerAdapter.
  DCHECK_EQ(this, window_->GetProperty(kAuraLayoutManagerAdapter));
  window_->ClearProperty(kAuraLayoutManagerAdapter);
}

// static
AuraLayoutManagerAdapter* AuraLayoutManagerAdapter::Get(aura::Window* window) {
  return window->GetProperty(kAuraLayoutManagerAdapter);
}

void AuraLayoutManagerAdapter::OnWindowResized() {
  wm_layout_manager_->OnWindowResized();
}

void AuraLayoutManagerAdapter::OnWindowAddedToLayout(aura::Window* child) {
  wm_layout_manager_->OnWindowAddedToLayout(WmWindow::Get(child));
}

void AuraLayoutManagerAdapter::OnWillRemoveWindowFromLayout(
    aura::Window* child) {
  wm_layout_manager_->OnWillRemoveWindowFromLayout(WmWindow::Get(child));
}

void AuraLayoutManagerAdapter::OnWindowRemovedFromLayout(aura::Window* child) {
  wm_layout_manager_->OnWindowRemovedFromLayout(WmWindow::Get(child));
}

void AuraLayoutManagerAdapter::OnChildWindowVisibilityChanged(
    aura::Window* child,
    bool visible) {
  wm_layout_manager_->OnChildWindowVisibilityChanged(WmWindow::Get(child),
                                                     visible);
}

void AuraLayoutManagerAdapter::SetChildBounds(
    aura::Window* child,
    const gfx::Rect& requested_bounds) {
  wm_layout_manager_->SetChildBounds(WmWindow::Get(child), requested_bounds);
}

}  // namespace ash
