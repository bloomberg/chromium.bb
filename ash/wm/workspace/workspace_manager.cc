// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_manager.h"

#include <algorithm>

#include "ash/wm/property_util.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/workspace.h"
#include "base/auto_reset.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/screen_aura.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/compositor/layer.h"
#include "ui/gfx/compositor/layer_animator.h"
#include "ui/gfx/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/transform.h"

namespace {

// The horizontal margein between workspaces in pixels.
const int kWorkspaceHorizontalMargin = 50;

// Minimum/maximum scale for overview mode.
const float kMaxOverviewScale = 0.9f;
const float kMinOverviewScale = 0.3f;

// Sets the visibility of the layer of each window in |windows| to |value|. We
// set the visibility of the layer rather than the Window so that views doesn't
// see the visibility change.
// TODO: revisit this. Ideally the Window would think it's still visibile after
// this. May be possible after Ben lands his changes.
void SetWindowLayerVisibility(const std::vector<aura::Window*>& windows,
                              bool value) {
  for (size_t i = 0; i < windows.size(); ++i){
    if (windows[i]->layer())
      windows[i]->layer()->SetVisible(value);
    SetWindowLayerVisibility(windows[i]->transient_children(), value);
  }
}

}

namespace ash {
namespace internal {

////////////////////////////////////////////////////////////////////////////////
// WindowManager, public:

WorkspaceManager::WorkspaceManager(aura::Window* contents_view)
    : contents_view_(contents_view),
      active_workspace_(NULL),
      workspace_size_(
          gfx::Screen::GetMonitorAreaNearestWindow(contents_view_).size()),
      is_overview_(false),
      ignored_window_(NULL) {
  DCHECK(contents_view);
}

WorkspaceManager::~WorkspaceManager() {
  for (size_t i = 0; i < workspaces_.size(); ++i) {
    Workspace* workspace = workspaces_[i];
    for (size_t j = 0; j < workspace->windows().size(); ++j)
      workspace->windows()[j]->RemoveObserver(this);
  }
  std::vector<Workspace*> copy_to_delete(workspaces_);
  STLDeleteElements(&copy_to_delete);
}

bool WorkspaceManager::IsManagedWindow(aura::Window* window) const {
  return window->type() == aura::client::WINDOW_TYPE_NORMAL &&
         !window->transient_parent();
}

void WorkspaceManager::AddWindow(aura::Window* window) {
  DCHECK(IsManagedWindow(window));

  if (FindBy(window))
    return;  // Already know about this window.

  window->AddObserver(this);

  if (!window->GetProperty(aura::client::kShowStateKey)) {
    // TODO: set maximized if width < x.
    window->SetIntProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  }

  // TODO: handle fullscreen.
  if (window_util::IsWindowMaximized(window)) {
    if (!GetRestoreBounds(window))
      SetRestoreBounds(window, window->GetTargetBounds());
    else
      SetWindowBounds(window, GetWorkAreaBounds());
  }

  Workspace* workspace = NULL;
  Workspace::Type type_for_window = Workspace::TypeForWindow(window);
  switch (type_for_window) {
    case Workspace::TYPE_SPLIT:
      // Splits either go in current workspace (if maximized or split). If the
      // current workspace isn't split/maximized, then create a maximized
      // workspace.
      workspace = GetActiveWorkspace();
      if (workspace &&
          (workspace->type() == Workspace::TYPE_SPLIT ||
           workspace->type() == Workspace::TYPE_MAXIMIZED)) {
        // TODO: this needs to reset bounds of any existing windows in
        // workspace.
        workspace->SetType(Workspace::TYPE_SPLIT);
      } else {
        type_for_window = Workspace::TYPE_MAXIMIZED;
        workspace = NULL;
      }
      break;

    case Workspace::TYPE_NORMAL:
      // All normal windows go in the same workspace.
      workspace = GetNormalWorkspace();
      break;

    case Workspace::TYPE_MAXIMIZED:
      // All maximized windows go in their own workspace.
      break;

    default:
      NOTREACHED();
      break;
  }

  if (!workspace) {
    workspace = new Workspace(this);
    workspace->SetType(type_for_window);
  }
  workspace->AddWindowAfter(window, NULL);
  workspace->Activate();
}

void WorkspaceManager::RemoveWindow(aura::Window* window) {
  Workspace* workspace = FindBy(window);
  if (!workspace)
    return;
  window->RemoveObserver(this);
  workspace->RemoveWindow(window);
  if (workspace->is_empty())
    delete workspace;
}

void WorkspaceManager::SetActiveWorkspaceByWindow(aura::Window* window) {
  Workspace* workspace = FindBy(window);
  if (workspace)
    workspace->Activate();
}

gfx::Rect WorkspaceManager::GetDragAreaBounds() {
  return GetWorkAreaBounds();
}

void WorkspaceManager::SetOverview(bool overview) {
  if (is_overview_ == overview)
    return;
  NOTIMPLEMENTED();
}

void WorkspaceManager::SetWorkspaceSize(const gfx::Size& workspace_size) {
  if (workspace_size == workspace_size_)
    return;
  workspace_size_ = workspace_size;
  for (Workspaces::const_iterator i = workspaces_.begin();
       i != workspaces_.end(); ++i) {
    (*i)->WorkspaceSizeChanged();
  }
}

void WorkspaceManager::OnWindowPropertyChanged(aura::Window* window,
                                               const char* name,
                                               void* old) {
  if (!IsManagedWindow(window))
    return;

  if (name != aura::client::kShowStateKey)
    return;
  // TODO: handle fullscreen.
  bool is_maximized = window->GetIntProperty(name) == ui::SHOW_STATE_MAXIMIZED;
  bool was_maximized =
      (old == reinterpret_cast<void*>(ui::SHOW_STATE_MAXIMIZED));
  if (is_maximized == was_maximized)
    return;

  MaximizedStateChanged(window);
}

////////////////////////////////////////////////////////////////////////////////
// WorkspaceManager, private:

void WorkspaceManager::AddWorkspace(Workspace* workspace) {
  DCHECK(std::find(workspaces_.begin(), workspaces_.end(),
                   workspace) == workspaces_.end());
  if (active_workspace_) {
    // New workspaces go right after current workspace.
    Workspaces::iterator i = std::find(workspaces_.begin(), workspaces_.end(),
                                       active_workspace_);
    workspaces_.insert(++i, workspace);
  } else {
    workspaces_.push_back(workspace);
  }
}

void WorkspaceManager::RemoveWorkspace(Workspace* workspace) {
  Workspaces::iterator i = std::find(workspaces_.begin(),
                                     workspaces_.end(),
                                     workspace);
  DCHECK(i != workspaces_.end());
  i = workspaces_.erase(i);
  if (active_workspace_ == workspace) {
    // TODO: need mru order.
    if (i != workspaces_.end())
      SetActiveWorkspace(*i);
    else if (!workspaces_.empty())
      SetActiveWorkspace(workspaces_.back());
    else
      active_workspace_ = NULL;
  }
}

Workspace* WorkspaceManager::GetActiveWorkspace() const {
  return active_workspace_;
}

Workspace* WorkspaceManager::FindBy(aura::Window* window) const {
  int index = GetWorkspaceIndexContaining(window);
  return index < 0 ? NULL : workspaces_[index];
}

void WorkspaceManager::SetActiveWorkspace(Workspace* workspace) {
  if (active_workspace_ == workspace)
    return;
  DCHECK(std::find(workspaces_.begin(), workspaces_.end(),
                   workspace) != workspaces_.end());
  if (active_workspace_)
    SetWindowLayerVisibility(active_workspace_->windows(), false);
  active_workspace_ = workspace;
  if (active_workspace_)
    SetWindowLayerVisibility(active_workspace_->windows(), true);

  is_overview_ = false;
}

gfx::Rect WorkspaceManager::GetWorkAreaBounds() {
  gfx::Rect bounds(workspace_size_);
  bounds.Inset(
      aura::RootWindow::GetInstance()->screen()->work_area_insets());
  return bounds;
}

// Returns the index of the workspace that contains the |window|.
int WorkspaceManager::GetWorkspaceIndexContaining(aura::Window* window) const {
  for (Workspaces::const_iterator i = workspaces_.begin();
       i != workspaces_.end();
       ++i) {
    if ((*i)->Contains(window))
      return i - workspaces_.begin();
  }
  return -1;
}

void WorkspaceManager::SetWindowBounds(aura::Window* window,
                                       const gfx::Rect& bounds) {
  // TODO: I suspect it's possible for this to be invoked when ignored_window_
  // is non-NULL.
  ignored_window_ = window;
  window->SetBounds(bounds);
  ignored_window_ = NULL;
}

void WorkspaceManager::SetWindowBoundsFromRestoreBounds(aura::Window* window) {
  Workspace* workspace = FindBy(window);
  DCHECK(workspace);
  const gfx::Rect* restore = GetRestoreBounds(window);
  if (restore) {
    SetWindowBounds(window,
                    restore->AdjustToFit(workspace->GetWorkAreaBounds()));
  } else {
    SetWindowBounds(window, window->bounds().AdjustToFit(
                        workspace->GetWorkAreaBounds()));
  }
  ash::ClearRestoreBounds(window);
}

void WorkspaceManager::MaximizedStateChanged(aura::Window* window) {
  DCHECK(IsManagedWindow(window));
  bool is_maximized = window_util::IsWindowMaximized(window);
  Workspace* current_workspace = FindBy(window);
  DCHECK(current_workspace);
  if (is_maximized) {
    // Unmaximized -> maximized; create a new workspace (unless current only has
    // one window).
    SetRestoreBounds(window, window->GetTargetBounds());
    if (current_workspace->num_windows() != 1) {
      current_workspace->RemoveWindow(window);
      Workspace* workspace = new Workspace(this);
      workspace->SetType(Workspace::TYPE_MAXIMIZED);
      workspace->AddWindowAfter(window, NULL);
      current_workspace = workspace;
    } else {
      current_workspace->SetType(Workspace::TYPE_MAXIMIZED);
    }
    SetWindowBounds(window, GetWorkAreaBounds());
  } else {
    // Maximized -> unmaximized; move window to unmaximized workspace (or reuse
    // current if there isn't one).
    window_util::SetOpenWindowSplit(window, false);
    Workspace* workspace = GetNormalWorkspace();
    if (workspace) {
      current_workspace->RemoveWindow(window);
      DCHECK(current_workspace->is_empty());
      workspace->AddWindowAfter(window, NULL);
      delete current_workspace;
      current_workspace = workspace;
    } else {
      current_workspace->SetType(Workspace::TYPE_NORMAL);
    }

    SetWindowBoundsFromRestoreBounds(window);
  }

  SetActiveWorkspace(current_workspace);
}

Workspace* WorkspaceManager::GetNormalWorkspace() {
  for (size_t i = 0; i < workspaces_.size(); ++i) {
    if (workspaces_[i]->type() == Workspace::TYPE_NORMAL)
      return workspaces_[i];
  }
  return NULL;
}

}  // namespace internal
}  // namespace ash
