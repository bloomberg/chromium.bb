// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/always_on_top_controller.h"

#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"

namespace ash {
namespace internal {

AlwaysOnTopController::AlwaysOnTopController()
    : default_container_(NULL),
      always_on_top_container_(NULL) {
}

AlwaysOnTopController::~AlwaysOnTopController() {
  if (default_container_)
    default_container_->RemoveObserver(this);
  if (always_on_top_container_)
    always_on_top_container_->RemoveObserver(this);
}

void AlwaysOnTopController::SetContainers(aura::Window* default_container,
    aura::Window* always_on_top_container) {
  // Both containers should have no children.
  DCHECK(default_container->children().empty());
  DCHECK(always_on_top_container->children().empty());

  // We are not handling any containers yet.
  DCHECK(default_container_ == NULL && always_on_top_container_ == NULL);

  default_container_ = default_container;
  default_container_->AddObserver(this);

  always_on_top_container_ = always_on_top_container;
  always_on_top_container_->AddObserver(this);
}

aura::Window* AlwaysOnTopController::GetContainer(aura::Window* window) const {
  DCHECK(default_container_ && always_on_top_container_);
  return !window->GetProperty(aura::client::kAlwaysOnTopKey) ?
      default_container_ : always_on_top_container_;
}

void AlwaysOnTopController::OnWindowAdded(aura::Window* child) {
  // Observe direct child of the containers.
  if (child->parent() == default_container_ ||
      child->parent() == always_on_top_container_) {
    child->AddObserver(this);
  }
}

void AlwaysOnTopController::OnWillRemoveWindow(aura::Window* child) {
  child->RemoveObserver(this);
}

void AlwaysOnTopController::OnWindowPropertyChanged(aura::Window* window,
                                                    const void* key,
                                                    intptr_t old) {
  if (key == aura::client::kAlwaysOnTopKey) {
    DCHECK(window->type() == aura::client::WINDOW_TYPE_NORMAL ||
           window->type() == aura::client::WINDOW_TYPE_POPUP);
    aura::Window* container = GetContainer(window);
    if (window->parent() != container)
      container->AddChild(window);
  }
}

void AlwaysOnTopController::OnWindowDestroyed(aura::Window* window) {
  if (window == default_container_)
    default_container_ = NULL;
  if (window == always_on_top_container_)
    always_on_top_container_ = NULL;
}

}  // namespace internal
}  // namespace ash
