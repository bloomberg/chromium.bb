// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/always_on_top_controller.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/wm/workspace/workspace_layout_manager.h"
#include "ash/wm_window.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"

namespace ash {

AlwaysOnTopController::AlwaysOnTopController(WmWindow* viewport)
    : always_on_top_container_(viewport) {
  DCHECK_NE(kShellWindowId_DefaultContainer, viewport->aura_window()->id());
  always_on_top_container_->aura_window()->SetLayoutManager(
      new WorkspaceLayoutManager(viewport->aura_window()));
  // Container should be empty.
  DCHECK(always_on_top_container_->GetChildren().empty());
  always_on_top_container_->aura_window()->AddObserver(this);
}

AlwaysOnTopController::~AlwaysOnTopController() {
  if (always_on_top_container_)
    always_on_top_container_->aura_window()->RemoveObserver(this);
}

WmWindow* AlwaysOnTopController::GetContainer(WmWindow* window) const {
  DCHECK(always_on_top_container_);
  if (window->aura_window()->GetProperty(aura::client::kAlwaysOnTopKey))
    return always_on_top_container_;
  return always_on_top_container_->GetRootWindow()->GetChildByShellWindowId(
      kShellWindowId_DefaultContainer);
}

// TODO(rsadam@): Refactor so that this cast is unneeded.
WorkspaceLayoutManager* AlwaysOnTopController::GetLayoutManager() const {
  return static_cast<WorkspaceLayoutManager*>(
      always_on_top_container_->aura_window()->layout_manager());
}

void AlwaysOnTopController::SetLayoutManagerForTest(
    std::unique_ptr<WorkspaceLayoutManager> layout_manager) {
  always_on_top_container_->aura_window()->SetLayoutManager(
      layout_manager.release());
}

void AlwaysOnTopController::OnWindowHierarchyChanged(
    const HierarchyChangeParams& params) {
  if (WmWindow::Get(params.old_parent) == always_on_top_container_)
    params.target->RemoveObserver(this);
  else if (WmWindow::Get(params.new_parent) == always_on_top_container_)
    params.target->AddObserver(this);
}

void AlwaysOnTopController::OnWindowPropertyChanged(aura::Window* window,
                                                    const void* key,
                                                    intptr_t old) {
  if (WmWindow::Get(window) != always_on_top_container_ &&
      key == aura::client::kAlwaysOnTopKey) {
    DCHECK(window->type() == ui::wm::WINDOW_TYPE_NORMAL ||
           window->type() == ui::wm::WINDOW_TYPE_POPUP);
    WmWindow* container = GetContainer(WmWindow::Get(window));
    if (WmWindow::Get(window->parent()) != container)
      container->AddChild(WmWindow::Get(window));
  }
}

void AlwaysOnTopController::OnWindowDestroying(aura::Window* window) {
  if (WmWindow::Get(window) == always_on_top_container_) {
    always_on_top_container_->aura_window()->RemoveObserver(this);
    always_on_top_container_ = nullptr;
  }
}

}  // namespace ash
