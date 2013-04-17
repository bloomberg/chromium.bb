// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_cycle_controller.h"

#include <algorithm>

#include "ash/shell_delegate.h"
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

// List of containers whose children we will cycle through.
const int kContainerIds[] = {
  internal::kShellWindowId_DefaultContainer,
  internal::kShellWindowId_AlwaysOnTopContainer
};

// Filter to watch for the termination of a keyboard gesture to cycle through
// multiple windows.
class WindowCycleEventFilter : public ui::EventHandler {
 public:
  WindowCycleEventFilter();
  virtual ~WindowCycleEventFilter();

  // Overridden from ui::EventHandler:
  virtual void OnKeyEvent(ui::KeyEvent* event) OVERRIDE;
 private:
  DISALLOW_COPY_AND_ASSIGN(WindowCycleEventFilter);
};

// Watch for all keyboard events by filtering the root window.
WindowCycleEventFilter::WindowCycleEventFilter() {
}

WindowCycleEventFilter::~WindowCycleEventFilter() {
}

void WindowCycleEventFilter::OnKeyEvent(ui::KeyEvent* event) {
  // Views uses VKEY_MENU for both left and right Alt keys.
  if (event->key_code() == ui::VKEY_MENU &&
      event->type() == ui::ET_KEY_RELEASED) {
    Shell::GetInstance()->window_cycle_controller()->AltKeyReleased();
    // Warning: |this| will be deleted from here on.
  }
}

// Adds all the children of |window| to |windows|.
void AddAllChildren(aura::Window* window,
                    WindowCycleList::WindowList* windows) {
  const WindowCycleList::WindowList& children(window->children());
  windows->insert(windows->end(), children.begin(), children.end());
}

// Adds all the children of all of |window|s children to |windows|.
void AddWorkspaceChildren(aura::Window* window,
                          WindowCycleList::WindowList* windows) {
  for (size_t i = 0; i < window->children().size(); ++i)
    AddAllChildren(window->children()[i], windows);
}

// Adds the windows that can be cycled through for the specified window id to
// |windows|.
void AddCycleWindows(aura::RootWindow* root,
                     int container_id,
                     WindowCycleList::WindowList* windows) {
  aura::Window* container = Shell::GetContainer(root, container_id);
  if (container_id == internal::kShellWindowId_DefaultContainer)
    AddWorkspaceChildren(container, windows);
  else
    AddAllChildren(container, windows);
}

}  // namespace

//////////////////////////////////////////////////////////////////////////////
// WindowCycleController, public:

WindowCycleController::WindowCycleController(
    aura::client::ActivationClient* activation_client)
    : activation_client_(activation_client) {
  activation_client_->AddObserver(this);
}

WindowCycleController::~WindowCycleController() {
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  for (Shell::RootWindowList::const_iterator iter = root_windows.begin();
       iter != root_windows.end(); ++iter) {
    for (size_t i = 0; i < arraysize(kContainerIds); ++i) {
      aura::Window* container = Shell::GetContainer(*iter, kContainerIds[i]);
      if (container)
        container->RemoveObserver(this);
    }
    aura::Window* default_container =
        Shell::GetContainer(*iter, internal::kShellWindowId_DefaultContainer);
    if (default_container) {
      for (size_t i = 0; i < default_container->children().size(); ++i) {
        aura::Window* workspace_window = default_container->children()[i];
        DCHECK_EQ(internal::kShellWindowId_WorkspaceContainer,
                  workspace_window->id());
        workspace_window->RemoveObserver(this);
      }
    }
  }

  activation_client_->RemoveObserver(this);
  StopCycling();
}

// static
bool WindowCycleController::CanCycle() {
  // Don't allow window cycling if the screen is locked or a modal dialog is
  // open.
  return !Shell::GetInstance()->IsScreenLocked() &&
         !Shell::GetInstance()->IsSystemModalWindowOpen();
}

void WindowCycleController::HandleCycleWindow(Direction direction,
                                              bool is_alt_down) {
  if (!CanCycle())
    return;

  if (is_alt_down) {
    if (!IsCycling()) {
      // This is the start of an alt-tab cycle through multiple windows, so
      // listen for the alt key being released to stop cycling.
      StartCycling();
      Step(direction);
      InstallEventFilter();
    } else {
      // We're in the middle of an alt-tab cycle, just step forward.
      Step(direction);
    }
  } else {
    // This is a simple, single-step window cycle.
    StartCycling();
    Step(direction);
    StopCycling();
  }
}

void WindowCycleController::HandleLinearCycleWindow() {
  if (!CanCycle() || IsCycling())
    return;

  // Use the reversed list of windows to prevent a 2-cycle of the most recent
  // windows occurring.
  WindowCycleList cycle_list(BuildWindowList(NULL,true));
  cycle_list.Step(WindowCycleList::FORWARD);
}

void WindowCycleController::AltKeyReleased() {
  StopCycling();
}

// static
std::vector<aura::Window*> WindowCycleController::BuildWindowList(
    const std::list<aura::Window*>* mru_windows,
    bool top_most_at_end) {
  WindowCycleList::WindowList windows;
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();

  aura::RootWindow* active_root = Shell::GetActiveRootWindow();
  for (Shell::RootWindowList::const_iterator iter = root_windows.begin();
       iter != root_windows.end(); ++iter) {
    if (*iter == active_root)
      continue;
    for (size_t i = 0; i < arraysize(kContainerIds); ++i)
      AddCycleWindows(*iter, kContainerIds[i], &windows);
  }

  // Add windows in the active root windows last so that the topmost window
  // in the active root window becomes the front of the list.
  for (size_t i = 0; i < arraysize(kContainerIds); ++i)
    AddCycleWindows(active_root, kContainerIds[i], &windows);

  // Removes unfocusable windows.
  WindowCycleList::WindowList::iterator last =
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
      WindowCycleList::WindowList::iterator window =
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

void WindowCycleController::OnRootWindowAdded(aura::RootWindow* root_window) {
  for (size_t i = 0; i < arraysize(kContainerIds); ++i) {
    aura::Window* container =
        Shell::GetContainer(root_window, kContainerIds[i]);
    container->AddObserver(this);
  }

  aura::Window* default_container =
      Shell::GetContainer(root_window,
                          internal::kShellWindowId_DefaultContainer);
  for (size_t i = 0; i < default_container->children().size(); ++i) {
    aura::Window* workspace_window = default_container->children()[i];
    DCHECK_EQ(internal::kShellWindowId_WorkspaceContainer,
              workspace_window->id());
    workspace_window->AddObserver(this);
  }
}

//////////////////////////////////////////////////////////////////////////////
// WindowCycleController, private:

void WindowCycleController::StartCycling() {
  windows_.reset(new WindowCycleList(BuildWindowList(&mru_windows_, false)));
}

void WindowCycleController::Step(Direction direction) {
  DCHECK(windows_.get());
  windows_->Step(direction == FORWARD ? WindowCycleList::FORWARD :
                 WindowCycleList::BACKWARD);
}

void WindowCycleController::StopCycling() {
  windows_.reset();
  // Remove our key event filter.
  if (event_handler_.get()) {
    Shell::GetInstance()->RemovePreTargetHandler(event_handler_.get());
    event_handler_.reset();
  }

  // Add the currently focused window to the MRU list
  aura::Window* active_window = wm::GetActiveWindow();
  mru_windows_.remove(active_window);
  mru_windows_.push_front(active_window);
}

// static
bool WindowCycleController::IsTrackedContainer(aura::Window* window) {
  if (!window)
    return false;
  for (size_t i = 0; i < arraysize(kContainerIds); ++i) {
    if (window->id() == kContainerIds[i]) {
      return true;
    }
  }
  return window->id() == internal::kShellWindowId_WorkspaceContainer;
}

void WindowCycleController::InstallEventFilter() {
  event_handler_.reset(new WindowCycleEventFilter());
  Shell::GetInstance()->AddPreTargetHandler(event_handler_.get());
}

void WindowCycleController::OnWindowActivated(aura::Window* gained_active,
                                              aura::Window* lost_active) {
  if (gained_active && !IsCycling() &&
      IsTrackedContainer(gained_active->parent())) {
    mru_windows_.remove(gained_active);
    mru_windows_.push_front(gained_active);
  }
}

void WindowCycleController::OnWindowAdded(aura::Window* window) {
  if (window->id() == internal::kShellWindowId_WorkspaceContainer)
    window->AddObserver(this);
}

void WindowCycleController::OnWillRemoveWindow(aura::Window* window) {
  mru_windows_.remove(window);
  if (window->id() == internal::kShellWindowId_WorkspaceContainer)
    window->RemoveObserver(this);
}

void WindowCycleController::OnWindowDestroying(aura::Window* window) {
  window->RemoveObserver(this);
}

}  // namespace ash
