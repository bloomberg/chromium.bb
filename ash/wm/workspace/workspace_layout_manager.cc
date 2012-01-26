// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_layout_manager.h"

#include "ash/wm/property_util.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/workspace.h"
#include "ash/wm/workspace/workspace_manager.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/event.h"
#include "ui/aura/root_window.h"
#include "ui/aura/screen_aura.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/rect.h"
#include "ui/views/widget/native_widget_aura.h"

namespace ash {
namespace internal {

////////////////////////////////////////////////////////////////////////////////
// WorkspaceLayoutManager, public:

WorkspaceLayoutManager::WorkspaceLayoutManager(
    WorkspaceManager* workspace_manager)
    : workspace_manager_(workspace_manager) {
}

WorkspaceLayoutManager::~WorkspaceLayoutManager() {}

void WorkspaceLayoutManager::PrepareForMoveOrResize(
    aura::Window* drag,
    aura::MouseEvent* event) {
  workspace_manager_->set_ignored_window(drag);
}

void WorkspaceLayoutManager::CancelMoveOrResize(
    aura::Window* drag,
    aura::MouseEvent* event) {
  workspace_manager_->set_ignored_window(NULL);
}

void WorkspaceLayoutManager::ProcessMove(
    aura::Window* drag,
    aura::MouseEvent* event) {
  // TODO: needs implementation for TYPE_SPLIT. For TYPE_SPLIT I want to
  // disallow eventfilter from moving and instead deal with it here.
}

void WorkspaceLayoutManager::EndMove(
    aura::Window* drag,
    aura::MouseEvent* evnet) {
  // TODO: see comment in ProcessMove.
  workspace_manager_->set_ignored_window(NULL);
}

void WorkspaceLayoutManager::EndResize(
    aura::Window* drag,
    aura::MouseEvent* evnet) {
  // TODO: see comment in ProcessMove.
  workspace_manager_->set_ignored_window(NULL);
}

////////////////////////////////////////////////////////////////////////////////
// WorkspaceLayoutManager, aura::LayoutManager implementation:

void WorkspaceLayoutManager::OnWindowResized() {
  // Workspace is updated via RootWindowObserver::OnRootWindowResized.
}

void WorkspaceLayoutManager::OnWindowAddedToLayout(aura::Window* child) {
  if (!workspace_manager_->IsManagedWindow(child))
    return;

  if (child->IsVisible())
    workspace_manager_->AddWindow(child);
}

void WorkspaceLayoutManager::OnWillRemoveWindowFromLayout(
    aura::Window* child) {
  ClearRestoreBounds(child);
  workspace_manager_->RemoveWindow(child);
}

void WorkspaceLayoutManager::OnChildWindowVisibilityChanged(
    aura::Window* child,
    bool visible) {
  if (!workspace_manager_->IsManagedWindow(child))
    return;
  if (visible)
    workspace_manager_->AddWindow(child);
  else
    workspace_manager_->RemoveWindow(child);
}

void WorkspaceLayoutManager::SetChildBounds(
    aura::Window* child,
    const gfx::Rect& requested_bounds) {
  // Allow setting the bounds for any window we don't care about, isn't visible,
  // or we're setting the bounds of. All other request are dropped on the floor.
  if (child == workspace_manager_->ignored_window() ||
      !workspace_manager_->IsManagedWindow(child) || !child->IsVisible() ||
      (!window_util::IsWindowMaximized(child) &&
       !window_util::IsWindowFullscreen(child))) {
    SetChildBoundsDirect(child, requested_bounds);
  }
}

}  // namespace internal
}  // namespace ash
