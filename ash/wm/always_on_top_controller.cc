// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/always_on_top_controller.h"

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"

namespace ash {

AlwaysOnTopController::AlwaysOnTopController()
    : always_on_top_container_(NULL) {
}

AlwaysOnTopController::~AlwaysOnTopController() {
  if (always_on_top_container_)
    always_on_top_container_->RemoveObserver(this);
}

void AlwaysOnTopController::SetAlwaysOnTopContainer(
    aura::Window* always_on_top_container) {
  // Container should be empty.
  DCHECK(always_on_top_container->children().empty());

  // We are not handling any containers yet.
  DCHECK(always_on_top_container_ == NULL);

  always_on_top_container_ = always_on_top_container;
  always_on_top_container_->AddObserver(this);
}

aura::Window* AlwaysOnTopController::GetContainer(aura::Window* window) const {
  DCHECK(always_on_top_container_);
  if (window->GetProperty(aura::client::kAlwaysOnTopKey))
    return always_on_top_container_;
  return Shell::GetContainer(always_on_top_container_->GetRootWindow(),
                             kShellWindowId_DefaultContainer);
}

void AlwaysOnTopController::OnWindowAdded(aura::Window* child) {
  // Observe direct child of the containers.
  if (child->parent() == always_on_top_container_)
    child->AddObserver(this);
}

void AlwaysOnTopController::OnWillRemoveWindow(aura::Window* child) {
  child->RemoveObserver(this);
}

void AlwaysOnTopController::OnWindowPropertyChanged(aura::Window* window,
                                                    const void* key,
                                                    intptr_t old) {
  if (key == aura::client::kAlwaysOnTopKey) {
    DCHECK(window->type() == ui::wm::WINDOW_TYPE_NORMAL ||
           window->type() == ui::wm::WINDOW_TYPE_POPUP);
    aura::Window* container = GetContainer(window);
    if (window->parent() != container)
      container->AddChild(window);
  }
}

void AlwaysOnTopController::OnWindowDestroyed(aura::Window* window) {
  if (window == always_on_top_container_)
    always_on_top_container_ = NULL;
}

}  // namespace ash
