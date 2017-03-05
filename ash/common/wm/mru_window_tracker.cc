// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/wm/mru_window_tracker.h"

#include <algorithm>

#include "ash/common/wm/focus_rules.h"
#include "ash/common/wm/switchable_windows.h"
#include "ash/common/wm/window_state.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "base/bind.h"
#include "ui/aura/window.h"

namespace ash {

namespace {

using CanActivateWindowPredicate = base::Callback<bool(WmWindow*)>;

bool CallCanActivate(WmWindow* window) {
  return window->CanActivate();
}

// Adds the windows that can be cycled through for the specified window id to
// |windows|.
void AddTrackedWindows(WmWindow* root,
                       int container_id,
                       MruWindowTracker::WindowList* windows) {
  WmWindow* container = root->GetChildByShellWindowId(container_id);
  const MruWindowTracker::WindowList children(container->GetChildren());
  windows->insert(windows->end(), children.begin(), children.end());
}

// Returns a list of windows ordered by their stacking order.
// If |mru_windows| is passed, these windows are moved to the front of the list.
// It uses the given |should_include_window_predicate| to determine whether to
// include a window in the returned list or not.
MruWindowTracker::WindowList BuildWindowListInternal(
    const std::list<WmWindow*>* mru_windows,
    const CanActivateWindowPredicate& should_include_window_predicate) {
  MruWindowTracker::WindowList windows;
  WmWindow* active_root = WmShell::Get()->GetRootWindowForNewWindows();
  for (WmWindow* window : WmShell::Get()->GetAllRootWindows()) {
    if (window == active_root)
      continue;
    for (size_t i = 0; i < wm::kSwitchableWindowContainerIdsLength; ++i)
      AddTrackedWindows(window, wm::kSwitchableWindowContainerIds[i], &windows);
  }

  // Add windows in the active root windows last so that the topmost window
  // in the active root window becomes the front of the list.
  for (size_t i = 0; i < wm::kSwitchableWindowContainerIdsLength; ++i)
    AddTrackedWindows(active_root, wm::kSwitchableWindowContainerIds[i],
                      &windows);

  // Removes unfocusable windows.
  std::vector<WmWindow*>::iterator itr = windows.begin();
  while (itr != windows.end()) {
    if (!should_include_window_predicate.Run(*itr))
      itr = windows.erase(itr);
    else
      ++itr;
  }

  // Put the windows in the mru_windows list at the head, if it's available.
  if (mru_windows) {
    // Iterate through the list backwards, so that we can move each window to
    // the front of the windows list as we find them.
    for (auto ix = mru_windows->rbegin(); ix != mru_windows->rend(); ++ix) {
      // Exclude windows in non-switchable containers and those which cannot
      // be activated.
      if (!wm::IsSwitchableContainer((*ix)->GetParent()) ||
          !should_include_window_predicate.Run(*ix)) {
        continue;
      }

      MruWindowTracker::WindowList::iterator window =
          std::find(windows.begin(), windows.end(), *ix);
      if (window != windows.end()) {
        windows.erase(window);
        windows.push_back(*ix);
      }
    }
  }

  // Window cycling expects the topmost window at the front of the list.
  std::reverse(windows.begin(), windows.end());

  return windows;
}

}  // namespace

//////////////////////////////////////////////////////////////////////////////
// MruWindowTracker, public:

MruWindowTracker::MruWindowTracker() : ignore_window_activations_(false) {
  WmShell::Get()->AddActivationObserver(this);
}

MruWindowTracker::~MruWindowTracker() {
  WmShell::Get()->RemoveActivationObserver(this);
  for (WmWindow* window : mru_windows_)
    window->aura_window()->RemoveObserver(this);
}

MruWindowTracker::WindowList MruWindowTracker::BuildMruWindowList() const {
  return BuildWindowListInternal(&mru_windows_, base::Bind(&CallCanActivate));
}

MruWindowTracker::WindowList MruWindowTracker::BuildWindowListIgnoreModal()
    const {
  return BuildWindowListInternal(nullptr,
                                 base::Bind(&IsWindowConsideredActivatable));
}

void MruWindowTracker::SetIgnoreActivations(bool ignore) {
  ignore_window_activations_ = ignore;

  // If no longer ignoring window activations, move currently active window
  // to front.
  if (!ignore)
    SetActiveWindow(WmShell::Get()->GetActiveWindow());
}

//////////////////////////////////////////////////////////////////////////////
// MruWindowTracker, private:

void MruWindowTracker::SetActiveWindow(WmWindow* active_window) {
  if (!active_window)
    return;

  std::list<WmWindow*>::iterator iter =
      std::find(mru_windows_.begin(), mru_windows_.end(), active_window);
  // Observe all newly tracked windows.
  if (iter == mru_windows_.end())
    active_window->aura_window()->AddObserver(this);
  else
    mru_windows_.erase(iter);
  mru_windows_.push_front(active_window);
}

void MruWindowTracker::OnWindowActivated(WmWindow* gained_active,
                                         WmWindow* lost_active) {
  if (!ignore_window_activations_)
    SetActiveWindow(gained_active);
}

void MruWindowTracker::OnWindowDestroyed(aura::Window* window) {
  // It's possible for OnWindowActivated() to be called after
  // OnWindowDestroying(). This means we need to override OnWindowDestroyed()
  // else we may end up with a deleted window in |mru_windows_|.
  mru_windows_.remove(WmWindow::Get(window));
  window->RemoveObserver(this);
}

}  // namespace ash
