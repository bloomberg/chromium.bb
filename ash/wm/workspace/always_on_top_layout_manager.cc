// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/always_on_top_layout_manager.h"

#include "ash/wm/window_animations.h"
#include "ash/wm/window_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/event.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_property.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/compositor/layer.h"
#include "ui/gfx/rect.h"

DECLARE_WINDOW_PROPERTY_TYPE(ui::WindowShowState)

namespace ash {
namespace internal {

namespace {

// Used to remember the show state before the window was minimized.
DEFINE_WINDOW_PROPERTY_KEY(
    ui::WindowShowState, kRestoreShowStateKey, ui::SHOW_STATE_DEFAULT);

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// AlwaysOnTopLayoutManager, public:

AlwaysOnTopLayoutManager::AlwaysOnTopLayoutManager(
    aura::RootWindow* root_window)
    : BaseLayoutManager(root_window) {
}

AlwaysOnTopLayoutManager::~AlwaysOnTopLayoutManager() {
}

void AlwaysOnTopLayoutManager::OnChildWindowVisibilityChanged(
    aura::Window* child,
    bool visible) {
  if (visible) {
    if (wm::IsWindowMinimized(child)) {
      // Attempting to show a minimized window. Unminimize it.
      child->SetProperty(aura::client::kShowStateKey,
                         child->GetProperty(kRestoreShowStateKey));
      child->ClearProperty(kRestoreShowStateKey);
    }
  }
}

void AlwaysOnTopLayoutManager::OnWindowPropertyChanged(aura::Window* window,
                                                     const void* key,
                                                     intptr_t old) {
  BaseLayoutManager::OnWindowPropertyChanged(window, key, old);
  if (key == aura::client::kShowStateKey)
    ShowStateChanged(window, static_cast<ui::WindowShowState>(old));
}

void AlwaysOnTopLayoutManager::ShowStateChanged(
    aura::Window* window,
    ui::WindowShowState last_show_state) {
  if (wm::IsWindowMinimized(window)) {
    // Save the previous show state so that we can correctly restore it.
    window->SetProperty(kRestoreShowStateKey, last_show_state);
    SetWindowVisibilityAnimationType(
        window, WINDOW_VISIBILITY_ANIMATION_TYPE_MINIMIZE);

    // Hide the window.
    window->Hide();
    // Activate another window.
    if (wm::IsActiveWindow(window))
      wm::DeactivateWindow(window);
    return;
  }
  // We can end up here if the window was minimized and we are transitioning
  // to another state. In that case the window is hidden.
  if ((window->TargetVisibility() ||
       last_show_state == ui::SHOW_STATE_MINIMIZED)) {
    if (!window->layer()->visible()) {
      // The layer may be hidden if the window was previously minimized. Make
      // sure it's visible.
      window->Show();
    }
    return;
  }
}

}  // namespace internal
}  // namespace ash
