// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/mru_window_tracker.h"

#include <algorithm>

#include "ash/session_state_delegate.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/activation_controller.h"
#include "ash/wm/window_cycle_list.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace_controller.h"
#include "ui/aura/root_window.h"
#include "ui/base/events/event.h"
#include "ui/base/events/event_handler.h"

namespace ash {

namespace {

// List of containers whose children we will allow switching to.
const int kContainerIds[] = {
  internal::kShellWindowId_DefaultContainer,
  internal::kShellWindowId_AlwaysOnTopContainer
};

// Adds the windows that can be cycled through for the specified window id to
// |windows|.
void AddTrackedWindows(aura::RootWindow* root,
                     int container_id,
                     MruWindowTracker::WindowList* windows) {
  aura::Window* container = Shell::GetContainer(root, container_id);
  const MruWindowTracker::WindowList& children(container->children());
  windows->insert(windows->end(), children.begin(), children.end());
}

// Returns a list of windows ordered by their stacking order.
// If |mru_windows| is passed, these windows are moved to the front of the list.
// If |top_most_at_end|, the list is returned in descending (bottom-most / least
// recently used) order.
MruWindowTracker::WindowList BuildWindowListInternal(
    const std::list<aura::Window*>* mru_windows,
    bool top_most_at_end) {
  MruWindowTracker::WindowList windows;
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();

  aura::RootWindow* active_root = Shell::GetActiveRootWindow();
  for (Shell::RootWindowList::const_iterator iter = root_windows.begin();
       iter != root_windows.end(); ++iter) {
    if (*iter == active_root)
      continue;
    for (size_t i = 0; i < arraysize(kContainerIds); ++i)
      AddTrackedWindows(*iter, kContainerIds[i], &windows);
  }

  // Add windows in the active root windows last so that the topmost window
  // in the active root window becomes the front of the list.
  for (size_t i = 0; i < arraysize(kContainerIds); ++i)
    AddTrackedWindows(active_root, kContainerIds[i], &windows);

  // Removes unfocusable windows.
  MruWindowTracker::WindowList::iterator last =
      std::remove_if(
          windows.begin(),
          windows.end(),
          std::not1(std::ptr_fun(ash::wm::CanActivateWindow)));
  windows.erase(last, windows.end());

  // Put the windows in the mru_windows list at the head, if it's available.
  if (mru_windows) {
    // Iterate through the list backwards, so that we can move each window to
    // the front of the windows list as we find them.
    for (std::list<aura::Window*>::const_reverse_iterator ix =
         mru_windows->rbegin();
         ix != mru_windows->rend(); ++ix) {
      MruWindowTracker::WindowList::iterator window =
          std::find(windows.begin(), windows.end(), *ix);
      if (window != windows.end()) {
        windows.erase(window);
        windows.push_back(*ix);
      }
    }
  }

  // Window cycling expects the topmost window at the front of the list.
  if (!top_most_at_end)
    std::reverse(windows.begin(), windows.end());

  return windows;
}

}  // namespace

//////////////////////////////////////////////////////////////////////////////
// MruWindowTracker, public:

MruWindowTracker::MruWindowTracker(
    aura::client::ActivationClient* activation_client)
    : activation_client_(activation_client),
      ignore_window_activations_(false) {
  activation_client_->AddObserver(this);
}

MruWindowTracker::~MruWindowTracker() {
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  for (Shell::RootWindowList::const_iterator iter = root_windows.begin();
       iter != root_windows.end(); ++iter) {
    for (size_t i = 0; i < arraysize(kContainerIds); ++i) {
      aura::Window* container = Shell::GetContainer(*iter, kContainerIds[i]);
      if (container)
        container->RemoveObserver(this);
    }
  }

  activation_client_->RemoveObserver(this);
}

// static
MruWindowTracker::WindowList MruWindowTracker::BuildWindowList(
    bool top_most_at_end) {
  return BuildWindowListInternal(NULL, top_most_at_end);
}

MruWindowTracker::WindowList MruWindowTracker::BuildMruWindowList() {
  return BuildWindowListInternal(&mru_windows_, false);
}

void MruWindowTracker::OnRootWindowAdded(aura::RootWindow* root_window) {
  for (size_t i = 0; i < arraysize(kContainerIds); ++i) {
    aura::Window* container =
        Shell::GetContainer(root_window, kContainerIds[i]);
    container->AddObserver(this);
  }
}

void MruWindowTracker::SetIgnoreActivations(bool ignore) {
  ignore_window_activations_ = ignore;

  // If no longer ignoring window activations, move currently active window
  // to front.
  if (!ignore) {
    aura::Window* active_window = wm::GetActiveWindow();
    mru_windows_.remove(active_window);
    mru_windows_.push_front(active_window);
  }
}

//////////////////////////////////////////////////////////////////////////////
// MruWindowTracker, private:

// static
bool MruWindowTracker::IsTrackedContainer(aura::Window* window) {
  if (!window)
    return false;
  for (size_t i = 0; i < arraysize(kContainerIds); ++i) {
    if (window->id() == kContainerIds[i])
      return true;
  }
  return false;
}

void MruWindowTracker::OnWindowActivated(aura::Window* gained_active,
                                         aura::Window* lost_active) {
  if (gained_active && !ignore_window_activations_ &&
      IsTrackedContainer(gained_active->parent())) {
    mru_windows_.remove(gained_active);
    mru_windows_.push_front(gained_active);
  }
}

void MruWindowTracker::OnWillRemoveWindow(aura::Window* window) {
  mru_windows_.remove(window);
}

void MruWindowTracker::OnWindowDestroying(aura::Window* window) {
  window->RemoveObserver(this);
}

}  // namespace ash
