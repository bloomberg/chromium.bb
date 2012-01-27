// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/visibility_controller.h"

#include "ash/wm/window_animations.h"
#include "ui/aura/window.h"

namespace ash {
namespace internal {
namespace {

// Property set on all windows whose child windows' visibility changes are
// animated. The type of the value is bool.
const char kChildWindowVisibilityChangesAnimated[] =
    "ash/wm/ChildWindowVisibilityChangesAnimated";

bool GetChildWindowVisibilityChangesAnimated(aura::Window* window) {
  if (!window)
    return false;
  return window->GetIntProperty(kChildWindowVisibilityChangesAnimated) != 0;
}

}  // namespace

VisibilityController::VisibilityController() {
}

VisibilityController::~VisibilityController() {
}

void VisibilityController::UpdateLayerVisibility(aura::Window* window,
                                                 bool visible) {
  bool animated = GetChildWindowVisibilityChangesAnimated(window->parent()) &&
                  window->type() != aura::client::WINDOW_TYPE_CONTROL &&
                  window->type() != aura::client::WINDOW_TYPE_UNKNOWN;
  if (animated)
    AnimateOnChildWindowVisibilityChanged(window, visible);

  // When a window is made visible, we always make its layer visible
  // immediately. When a window is hidden, the layer must be left visible and
  // only made not visible once the animation is complete.
  if (!animated || visible)
    window->layer()->SetVisible(visible);
}

}  // namespace internal

void SetChildWindowVisibilityChangesAnimated(aura::Window* window) {
  window->SetIntProperty(internal::kChildWindowVisibilityChangesAnimated, 1);
}

}  // namespace ash
