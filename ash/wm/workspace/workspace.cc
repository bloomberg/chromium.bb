// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace.h"

#include <algorithm>

#include "ash/wm/property_util.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/workspace_manager.h"
#include "base/logging.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_types.h"

namespace ash {
namespace internal {

Workspace::Workspace(WorkspaceManager* manager, Type type)
    : type_(type),
      workspace_manager_(manager) {
}

Workspace::~Workspace() {
  workspace_manager_->RemoveWorkspace(this);
}

// static
Workspace::Type Workspace::TypeForWindow(aura::Window* window) {
  if (wm::IsWindowMaximized(window) || wm::IsWindowFullscreen(window))
    return TYPE_MAXIMIZED;
  return TYPE_MANAGED;
}

bool Workspace::AddWindowAfter(aura::Window* window, aura::Window* after) {
  if (!CanAdd(window))
    return false;
  DCHECK(!Contains(window));

  aura::Window::Windows::iterator i =
      std::find(windows_.begin(), windows_.end(), after);
  if (!after || i == windows_.end())
    windows_.push_back(window);
  else
    windows_.insert(++i, window);
  OnWindowAddedAfter(window, after);
  return true;
}

void Workspace::RemoveWindow(aura::Window* window) {
  DCHECK(Contains(window));
  windows_.erase(std::find(windows_.begin(), windows_.end(), window));
  OnWindowRemoved(window);
}

bool Workspace::Contains(aura::Window* window) const {
  return std::find(windows_.begin(), windows_.end(), window) != windows_.end();
}

void Workspace::Activate() {
  workspace_manager_->SetActiveWorkspace(this);
}

void Workspace::SetWindowBounds(aura::Window* window, const gfx::Rect& bounds) {
  workspace_manager_->SetWindowBounds(window, bounds);
}

}  // namespace internal
}  // namespace ash
