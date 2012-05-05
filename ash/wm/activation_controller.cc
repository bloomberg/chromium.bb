// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/activation_controller.h"

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/window_modality_controller.h"
#include "ash/wm/window_util.h"
#include "base/auto_reset.h"
#include "ui/aura/client/activation_delegate.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/layer.h"

namespace ash {
namespace internal {
namespace {

// These are the list of container ids of containers which may contain windows
// that need to be activated in the order that they should be activated.
const int kWindowContainerIds[] = {
    kShellWindowId_LockSystemModalContainer,
    kShellWindowId_SettingBubbleContainer,
    kShellWindowId_LockScreenContainer,
    kShellWindowId_SystemModalContainer,
    kShellWindowId_AlwaysOnTopContainer,
    kShellWindowId_AppListContainer,
    kShellWindowId_DefaultContainer,

    // Panel, launcher and status are intentionally checked after other
    // containers even though these layers are higher. The user expects their
    // windows to be focused before these elements.
    kShellWindowId_PanelContainer,
    kShellWindowId_LauncherContainer,
    kShellWindowId_StatusContainer,
};

aura::Window* GetContainer(int id) {
  return Shell::GetInstance()->GetContainer(id);
}

// Returns true if children of |window| can be activated.
// These are the only containers in which windows can receive focus.
bool SupportsChildActivation(aura::Window* window) {
  for (size_t i = 0; i < arraysize(kWindowContainerIds); i++) {
    if (window->id() == kWindowContainerIds[i])
      return true;
  }
  return false;
}

bool HasModalTransientChild(aura::Window* window) {
  aura::Window::Windows::const_iterator it;
  for (it = window->transient_children().begin();
       it != window->transient_children().end();
       ++it) {
    if ((*it)->GetProperty(aura::client::kModalKey) == ui::MODAL_TYPE_WINDOW)
      return true;
  }
  return false;
}

// See description in VisibilityMatches.
enum ActivateVisibilityType {
  TARGET_VISIBILITY,
  CURRENT_VISIBILITY,
};

// Used by CanActivateWindowWithEvent() to test the visibility of a window.
// This is used by two distinct code paths:
// . when activating from an event we only care about the actual visibility.
// . when activating because of a keyboard accelerator, in which case we
//   care about the TargetVisibility.
bool VisibilityMatches(aura::Window* window, ActivateVisibilityType type) {
  bool visible = (type == CURRENT_VISIBILITY) ? window->IsVisible() :
      window->TargetVisibility();
  return visible || wm::IsWindowMinimized(window);
}

// Returns true if |window| can be activated or deactivated.
// A window manager typically defines some notion of "top level window" that
// supports activation/deactivation.
bool CanActivateWindowWithEvent(aura::Window* window,
                                const aura::Event* event,
                                ActivateVisibilityType visibility_type) {
  return window &&
      VisibilityMatches(window, visibility_type) &&
      (!aura::client::GetActivationDelegate(window) ||
        aura::client::GetActivationDelegate(window)->ShouldActivate(event)) &&
      SupportsChildActivation(window->parent());
}

// When a modal window is activated, we bring its entire transient parent chain
// to the front. This function must be called before the modal transient is
// stacked at the top to ensure correct stacking order.
void StackTransientParentsBelowModalWindow(aura::Window* window) {
  if (window->GetProperty(aura::client::kModalKey) != ui::MODAL_TYPE_WINDOW)
    return;

  aura::Window* transient_parent = window->transient_parent();
  while (transient_parent) {
    transient_parent->parent()->StackChildAtTop(transient_parent);
    transient_parent = transient_parent->transient_parent();
  }
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ActivationController, public:

ActivationController::ActivationController()
    : updating_activation_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(observer_manager_(this)) {
  aura::client::SetActivationClient(Shell::GetRootWindow(), this);
  aura::Env::GetInstance()->AddObserver(this);
  Shell::GetRootWindow()->AddRootWindowObserver(this);
}

ActivationController::~ActivationController() {
  Shell::GetRootWindow()->RemoveRootWindowObserver(this);
  aura::Env::GetInstance()->RemoveObserver(this);
}

// static
aura::Window* ActivationController::GetActivatableWindow(
    aura::Window* window,
    const aura::Event* event) {
  aura::Window* parent = window->parent();
  aura::Window* child = window;
  while (parent) {
    if (CanActivateWindowWithEvent(child, event, CURRENT_VISIBILITY))
      return child;
    // If |child| isn't activatable, but has transient parent, trace
    // that path instead.
    if (child->transient_parent())
      return GetActivatableWindow(child->transient_parent(), event);
    parent = parent->parent();
    child = child->parent();
  }
  return NULL;
}

bool ActivationController::CanActivateWindow(aura::Window* window) const {
  return CanActivateWindowWithEvent(window, NULL, TARGET_VISIBILITY) &&
         !HasModalTransientChild(window);
}

////////////////////////////////////////////////////////////////////////////////
// ActivationController, aura::client::ActivationClient implementation:

void ActivationController::ActivateWindow(aura::Window* window) {
  ActivateWindowWithEvent(window, NULL);
}

void ActivationController::DeactivateWindow(aura::Window* window) {
  if (window)
    ActivateNextWindow(window);
}

aura::Window* ActivationController::GetActiveWindow() {
  return Shell::GetRootWindow()->GetProperty(
      aura::client::kRootWindowActiveWindowKey);
}

bool ActivationController::OnWillFocusWindow(aura::Window* window,
                                             const aura::Event* event) {
  return CanActivateWindowWithEvent(
      GetActivatableWindow(window, event), event, CURRENT_VISIBILITY);
}

////////////////////////////////////////////////////////////////////////////////
// ActivationController, aura::WindowObserver implementation:

void ActivationController::OnWindowVisibilityChanged(aura::Window* window,
                                                     bool visible) {
  if (!visible) {
    aura::Window* next_window = ActivateNextWindow(window);
    if (next_window && next_window->parent() == window->parent()) {
      // Despite the activation change, we need to keep the window being hidden
      // stacked above the new window so it stays on top as it animates away.
      window->layer()->parent()->StackAbove(window->layer(),
                                            next_window->layer());
    }
  }
}

void ActivationController::OnWindowDestroying(aura::Window* window) {
  if (wm::IsActiveWindow(window)) {
    // Clear the property before activating something else, since
    // ActivateWindow() will attempt to notify the window stored in this value
    // otherwise.
    Shell::GetRootWindow()->ClearProperty(
        aura::client::kRootWindowActiveWindowKey);
    ActivateWindow(GetTopmostWindowToActivate(window));
  }
  observer_manager_.Remove(window);
}

////////////////////////////////////////////////////////////////////////////////
// ActivationController, aura::EnvObserver implementation:

void ActivationController::OnWindowInitialized(aura::Window* window) {
  observer_manager_.Add(window);
}

////////////////////////////////////////////////////////////////////////////////
// ActivationController, aura::RootWindowObserver implementation:

void ActivationController::OnWindowFocused(aura::Window* window) {
  ActivateWindow(GetActivatableWindow(window, NULL));
}

////////////////////////////////////////////////////////////////////////////////
// ActivationController, private:

void ActivationController::ActivateWindowWithEvent(aura::Window* window,
                                                   const aura::Event* event) {
  aura::Window* window_modal_transient = wm::GetWindowModalTransient(window);
  if (window_modal_transient) {
    ActivateWindow(window_modal_transient);
    return;
  }

  // Prevent recursion when called from focus.
  if (updating_activation_)
    return;

  AutoReset<bool> in_activate_window(&updating_activation_, true);
  // Nothing may actually have changed.
  aura::Window* old_active = GetActiveWindow();
  if (old_active == window)
    return;
  // The stacking client may impose rules on what window configurations can be
  // activated or deactivated.
  if (window && !CanActivateWindowWithEvent(window, event, CURRENT_VISIBILITY))
    return;

  // Restore minimized window. This needs to be done before CanReceiveEvents()
  // is called as that function checks window visibility.
  if (window && wm::IsWindowMinimized(window))
    window->Show();

  // If the screen is locked, just bring the window to top so that
  // it will be activated when the lock window is destroyed.
  if (window && !window->CanReceiveEvents()) {
    StackTransientParentsBelowModalWindow(window);
    window->parent()->StackChildAtTop(window);
    return;
  }
  if (window &&
      !window->Contains(window->GetFocusManager()->GetFocusedWindow())) {
    window->GetFocusManager()->SetFocusedWindow(window, event);
  }
  Shell::GetRootWindow()->SetProperty(aura::client::kRootWindowActiveWindowKey,
                                      window);
  // Invoke OnLostActive after we've changed the active window. That way if the
  // delegate queries for active state it doesn't think the window is still
  // active.
  if (old_active && aura::client::GetActivationDelegate(old_active))
    aura::client::GetActivationDelegate(old_active)->OnLostActive();

  if (window) {
    StackTransientParentsBelowModalWindow(window);
    window->parent()->StackChildAtTop(window);
    if (aura::client::GetActivationDelegate(window))
      aura::client::GetActivationDelegate(window)->OnActivated();
  }
}

aura::Window* ActivationController::ActivateNextWindow(aura::Window* window) {
  aura::Window* next_window = NULL;
  if (wm::IsActiveWindow(window)) {
    next_window = GetTopmostWindowToActivate(window);
    ActivateWindow(next_window);
  }
  return next_window;
}

aura::Window* ActivationController::GetTopmostWindowToActivate(
    aura::Window* ignore) const {
  size_t current_container_index = 0;
  // If the container of the window losing focus is in the list, start from that
  // container.
  for (size_t i = 0; ignore && i < arraysize(kWindowContainerIds); i++) {
    aura::Window* container = GetContainer(kWindowContainerIds[i]);
    if (container && container->Contains(ignore)) {
      current_container_index = i;
      break;
    }
  }

  // Look for windows to focus in that container and below.
  aura::Window* window = NULL;
  for (; !window && current_container_index < arraysize(kWindowContainerIds);
       current_container_index++) {
    aura::Window* container =
        GetContainer(kWindowContainerIds[current_container_index]);
    if (container)
      window = GetTopmostWindowToActivateInContainer(container, ignore);
  }
  return window;
}

aura::Window* ActivationController::GetTopmostWindowToActivateInContainer(
    aura::Window* container,
    aura::Window* ignore) const {
  for (aura::Window::Windows::const_reverse_iterator i =
           container->children().rbegin();
       i != container->children().rend();
       ++i) {
    if (*i != ignore &&
        CanActivateWindowWithEvent(*i, NULL, CURRENT_VISIBILITY) &&
        !wm::IsWindowMinimized(*i))
      return *i;
  }
  return NULL;
}

}  // namespace internal
}  // namespace ash
