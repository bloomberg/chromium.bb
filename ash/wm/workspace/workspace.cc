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

Workspace::Workspace(WorkspaceManager* manager)
    : type_(TYPE_NORMAL),
      workspace_manager_(manager) {
  workspace_manager_->AddWorkspace(this);
}

Workspace::~Workspace() {
  workspace_manager_->RemoveWorkspace(this);
}

// static
Workspace::Type Workspace::TypeForWindow(aura::Window* window) {
  if (window_util::GetOpenWindowSplit(window))
    return TYPE_SPLIT;
  if (window_util::IsWindowMaximized(window) ||
      window_util::IsWindowFullscreen(window)) {
    return TYPE_MAXIMIZED;
  }
  return TYPE_NORMAL;
}

void Workspace::SetType(Type type) {
  // Can only change the type when there are no windows, or the type of window
  // matches the type changing to. We need only check the first window as CanAdd
  // only allows new windows if the type matches.
  DCHECK(windows_.empty() || TypeForWindow(windows_[0]) == type);
  type_ = type;
}

void Workspace::WorkspaceSizeChanged() {
  if (!windows_.empty()) {
    // TODO: need to handle size changing.
    NOTIMPLEMENTED();
  }
}

gfx::Rect Workspace::GetWorkAreaBounds() const {
  return workspace_manager_->GetWorkAreaBounds();
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

  if (type_ == TYPE_MAXIMIZED) {
    workspace_manager_->SetWindowBounds(window, GetWorkAreaBounds());
  } else if (type_ == TYPE_SPLIT) {
    // TODO: this needs to adjust bounds appropriately.
    workspace_manager_->SetWindowBounds(window, GetWorkAreaBounds());
  }

  return true;
}

void Workspace::RemoveWindow(aura::Window* window) {
  DCHECK(Contains(window));
  windows_.erase(std::find(windows_.begin(), windows_.end(), window));
  // TODO: this needs to adjust things.
}

bool Workspace::Contains(aura::Window* window) const {
  return std::find(windows_.begin(), windows_.end(), window) != windows_.end();
}

void Workspace::Activate() {
  workspace_manager_->SetActiveWorkspace(this);
}

bool Workspace::ContainsFullscreenWindow() const {
  for (aura::Window::Windows::const_iterator i = windows_.begin();
       i != windows_.end();
       ++i) {
    aura::Window* w = *i;
    if (w->IsVisible() &&
        w->GetIntProperty(aura::client::kShowStateKey) ==
            ui::SHOW_STATE_FULLSCREEN)
      return true;
  }
  return false;
}

int Workspace::GetIndexOf(aura::Window* window) const {
  aura::Window::Windows::const_iterator i =
      std::find(windows_.begin(), windows_.end(), window);
  return i == windows_.end() ? -1 : i - windows_.begin();
}

bool Workspace::CanAdd(aura::Window* window) const {
  return TypeForWindow(window) == type_;
}

}  // namespace internal
}  // namespace ash
