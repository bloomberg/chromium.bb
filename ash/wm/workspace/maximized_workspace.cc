// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/maximized_workspace.h"

#include "ash/screen_ash.h"
#include "ash/wm/property_util.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/workspace_manager.h"
#include "base/logging.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/screen.h"

namespace ash {
namespace internal {

MaximizedWorkspace::MaximizedWorkspace(WorkspaceManager* manager)
    : Workspace(manager, TYPE_MAXIMIZED) {
}

MaximizedWorkspace::~MaximizedWorkspace() {
}

bool MaximizedWorkspace::CanAdd(aura::Window* window) const {
  return is_empty() && (wm::IsWindowFullscreen(window) ||
                        wm::IsWindowMaximized(window));
}

void MaximizedWorkspace::OnWindowAddedAfter(aura::Window* window,
                                            aura::Window* after) {
  ResetWindowBounds(window);
}

void MaximizedWorkspace::OnWindowRemoved(aura::Window* window) {
}

void MaximizedWorkspace::ResetWindowBounds(aura::Window* window) {
  if (wm::IsWindowFullscreen(window)) {
    SetWindowBounds(window,
                    gfx::Screen::GetDisplayNearestWindow(window).bounds());
  } else {
    SetWindowBounds(window, ScreenAsh::GetMaximizedWindowBounds(window));
  }
}

}  // namespace internal
}  // namespace ash
