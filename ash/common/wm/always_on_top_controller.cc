// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/wm/always_on_top_controller.h"

#include "ash/common/shell_window_ids.h"
#include "ash/common/wm/workspace/workspace_layout_manager.h"
#include "ash/common/wm/workspace/workspace_layout_manager_delegate.h"
#include "ash/common/wm_window.h"
#include "ash/common/wm_window_property.h"
#include "base/memory/ptr_util.h"

namespace ash {

AlwaysOnTopController::AlwaysOnTopController(WmWindow* viewport)
    : always_on_top_container_(viewport) {
  always_on_top_container_->SetLayoutManager(
      base::WrapUnique(new WorkspaceLayoutManager(viewport, nullptr)));
  // Container should be empty.
  DCHECK(always_on_top_container_->GetChildren().empty());
  always_on_top_container_->AddObserver(this);
}

AlwaysOnTopController::~AlwaysOnTopController() {
  if (always_on_top_container_)
    always_on_top_container_->RemoveObserver(this);
}

WmWindow* AlwaysOnTopController::GetContainer(WmWindow* window) const {
  DCHECK(always_on_top_container_);
  if (window->GetBoolProperty(WmWindowProperty::ALWAYS_ON_TOP))
    return always_on_top_container_;
  return always_on_top_container_->GetRootWindow()->GetChildByShellWindowId(
      kShellWindowId_DefaultContainer);
}

// TODO(rsadam@): Refactor so that this cast is unneeded.
WorkspaceLayoutManager* AlwaysOnTopController::GetLayoutManager() const {
  return static_cast<WorkspaceLayoutManager*>(
      always_on_top_container_->GetLayoutManager());
}

void AlwaysOnTopController::SetLayoutManagerForTest(
    std::unique_ptr<WorkspaceLayoutManager> layout_manager) {
  always_on_top_container_->SetLayoutManager(std::move(layout_manager));
}

void AlwaysOnTopController::OnWindowTreeChanged(
    WmWindow* window,
    const TreeChangeParams& params) {
  if (params.old_parent == always_on_top_container_)
    params.target->RemoveObserver(this);
  else if (params.new_parent == always_on_top_container_)
    params.target->AddObserver(this);
}

void AlwaysOnTopController::OnWindowPropertyChanged(WmWindow* window,
                                                    WmWindowProperty property) {
  if (window != always_on_top_container_ &&
      property == WmWindowProperty::ALWAYS_ON_TOP) {
    DCHECK(window->GetType() == ui::wm::WINDOW_TYPE_NORMAL ||
           window->GetType() == ui::wm::WINDOW_TYPE_POPUP);
    WmWindow* container = GetContainer(window);
    if (window->GetParent() != container)
      container->AddChild(window);
  }
}

void AlwaysOnTopController::OnWindowDestroying(WmWindow* window) {
  if (window == always_on_top_container_) {
    always_on_top_container_->RemoveObserver(this);
    always_on_top_container_ = nullptr;
  }
}

}  // namespace ash
