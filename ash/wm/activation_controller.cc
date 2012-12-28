// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/activation_controller.h"

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/activation_controller_delegate.h"
#include "ash/wm/property_util.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace_controller.h"
#include "base/auto_reset.h"
#include "ui/aura/client/activation_change_observer.h"
#include "ui/aura/client/activation_delegate.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/layer.h"
#include "ui/views/corewm/window_modality_controller.h"

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

bool BelongsToContainerWithEqualOrGreaterId(const aura::Window* window,
                                            int container_id) {
  for (; window; window = window->parent()) {
    if (window->id() >= container_id)
      return true;
  }
  return false;
}

// Returns true if children of |window| can be activated.
// These are the only containers in which windows can receive focus.
bool SupportsChildActivation(aura::Window* window) {
  if (window->id() == kShellWindowId_WorkspaceContainer)
    return true;

  for (size_t i = 0; i < arraysize(kWindowContainerIds); i++) {
    if (window->id() == kWindowContainerIds[i] &&
        window->id() != kShellWindowId_DefaultContainer) {
      return true;
    }
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
  return visible || wm::IsWindowMinimized(window) ||
      (window->TargetVisibility() &&
        (window->parent()->id() == kShellWindowId_WorkspaceContainer ||
         window->parent()->id() == kShellWindowId_LockScreenContainer));
}

// Returns true if |window| can be activated or deactivated.
// A window manager typically defines some notion of "top level window" that
// supports activation/deactivation.
bool CanActivateWindowWithEvent(aura::Window* window,
                                const ui::Event* event,
                                ActivateVisibilityType visibility_type) {
  return window &&
      VisibilityMatches(window, visibility_type) &&
      (!aura::client::GetActivationDelegate(window) ||
        aura::client::GetActivationDelegate(window)->ShouldActivate()) &&
      SupportsChildActivation(window->parent()) &&
      (BelongsToContainerWithEqualOrGreaterId(
          window, kShellWindowId_SystemModalContainer) ||
       !Shell::GetInstance()->IsSystemModalWindowOpen());
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

aura::Window* FindFocusableWindowFor(aura::Window* window) {
  while (window && !window->CanFocus())
    window = window->parent();
  return window;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ActivationController, public:

ActivationController::ActivationController(
    aura::client::FocusClient* focus_client,
    ActivationControllerDelegate* delegate)
    : focus_client_(focus_client),
      updating_activation_(false),
      active_window_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(observer_manager_(this)),
      delegate_(delegate) {
  aura::Env::GetInstance()->AddObserver(this);
  focus_client_->AddObserver(this);
}

ActivationController::~ActivationController() {
  aura::Env::GetInstance()->RemoveObserver(this);
  focus_client_->RemoveObserver(this);
}

// static
aura::Window* ActivationController::GetActivatableWindow(
    aura::Window* window,
    const ui::Event* event) {
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

void ActivationController::AddObserver(
    aura::client::ActivationChangeObserver* observer) {
  observers_.AddObserver(observer);
}

void ActivationController::RemoveObserver(
    aura::client::ActivationChangeObserver* observer) {
  observers_.RemoveObserver(observer);
}

void ActivationController::ActivateWindow(aura::Window* window) {
  ActivateWindowWithEvent(window, NULL);
}

void ActivationController::DeactivateWindow(aura::Window* window) {
  if (window)
    ActivateNextWindow(window);
}

aura::Window* ActivationController::GetActiveWindow() {
  return active_window_;
}

aura::Window* ActivationController::GetActivatableWindow(aura::Window* window) {
  return GetActivatableWindow(window, NULL);
}

aura::Window* ActivationController::GetToplevelWindow(aura::Window* window) {
  return GetActivatableWindow(window, NULL);
}

bool ActivationController::OnWillFocusWindow(aura::Window* window,
                                             const ui::Event* event) {
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
  // Don't use wm::IsActiveWidnow in case the |window| has already been
  // removed from the root tree.
  if (active_window_ == window) {
    active_window_ = NULL;
    FOR_EACH_OBSERVER(aura::client::ActivationChangeObserver,
                      observers_,
                      OnWindowActivated(NULL, window));
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

void ActivationController::OnWindowFocused(aura::Window* gained_focus,
                                           aura::Window* lost_focus) {
  if (gained_focus)
    ActivateWindow(GetActivatableWindow(gained_focus, NULL));
}

////////////////////////////////////////////////////////////////////////////////
// ActivationController, ui::EventHandler implementation:

void ActivationController::OnKeyEvent(ui::KeyEvent* event) {
}

void ActivationController::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() == ui::ET_MOUSE_PRESSED)
    FocusWindowWithEvent(event);
}

void ActivationController::OnScrollEvent(ui::ScrollEvent* event) {
}

void ActivationController::OnTouchEvent(ui::TouchEvent* event) {
  if (event->type() == ui::ET_TOUCH_PRESSED)
    FocusWindowWithEvent(event);
}

void ActivationController::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_BEGIN &&
      event->details().touch_points() == 1) {
    FocusWindowWithEvent(event);
  }
}

////////////////////////////////////////////////////////////////////////////////
// ActivationController, private:

void ActivationController::ActivateWindowWithEvent(aura::Window* window,
                                                   const ui::Event* event) {
  // Prevent recursion when called from focus.
  if (updating_activation_)
    return;
  base::AutoReset<bool> in_activate_window(&updating_activation_, true);

  // We allow the delegate to change which window gets activated, or to prevent
  // activation changes.
  aura::Window* original_active_window = window;
  window = delegate_->WillActivateWindow(window);
  // TODO(beng): note that this breaks the previous behavior where an activation
  //             attempt by a window behind the lock screen would at least
  //             restack that window frontmost within its container. fix this.
  if (!window && original_active_window != window)
    return;

  // TODO(beng): This encapsulates additional Ash-specific restrictions on
  //             whether activation can change. Should move to the delegate.
  if (window && !CanActivateWindowWithEvent(window, event, CURRENT_VISIBILITY))
    return;

  if (active_window_ == window)
    return;

  aura::Window* old_active = active_window_;
  active_window_ = window;

  if (window &&
      !window->Contains(aura::client::GetFocusClient(window)->
          GetFocusedWindow())) {
    aura::client::GetFocusClient(window)->FocusWindow(window, event);
  }

  if (window) {
    StackTransientParentsBelowModalWindow(window);
    window->parent()->StackChildAtTop(window);
  }

  FOR_EACH_OBSERVER(aura::client::ActivationChangeObserver,
                    observers_,
                    OnWindowActivated(window, old_active));
  if (aura::client::GetActivationChangeObserver(old_active)) {
    aura::client::GetActivationChangeObserver(old_active)->OnWindowActivated(
        window, old_active);
  }
  if (aura::client::GetActivationChangeObserver(window)) {
    aura::client::GetActivationChangeObserver(window)->OnWindowActivated(
        window, old_active);
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
  aura::RootWindow* root = ignore->GetRootWindow();
  if (!root)
    root = Shell::GetActiveRootWindow();
  for (size_t i = 0; ignore && i < arraysize(kWindowContainerIds); i++) {
    aura::Window* container = Shell::GetContainer(root, kWindowContainerIds[i]);
    if (container && container->Contains(ignore)) {
      current_container_index = i;
      break;
    }
  }

  // Look for windows to focus in that container and below.
  aura::Window* window = NULL;
  for (; !window && current_container_index < arraysize(kWindowContainerIds);
       current_container_index++) {
    aura::Window::Windows containers = Shell::GetContainersFromAllRootWindows(
        kWindowContainerIds[current_container_index],
        root);
    for (aura::Window::Windows::const_iterator iter = containers.begin();
         iter != containers.end() && !window; ++iter) {
      window = GetTopmostWindowToActivateInContainer((*iter), ignore);
    }
  }
  return window;
}

aura::Window* ActivationController::GetTopmostWindowToActivateInContainer(
    aura::Window* container,
    aura::Window* ignore) const {
  // Workspace has an extra level of windows that needs to be special cased.
  if (container->id() == kShellWindowId_DefaultContainer) {
    for (aura::Window::Windows::const_reverse_iterator i =
             container->children().rbegin();
         i != container->children().rend(); ++i) {
      if ((*i)->IsVisible()) {
        aura::Window* window = GetTopmostWindowToActivateInContainer(
            *i, ignore);
        if (window)
          return window;
      }
    }
    return NULL;
  }
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

void ActivationController::FocusWindowWithEvent(const ui::Event* event) {
  aura::Window* window = static_cast<aura::Window*>(event->target());
  window = delegate_->WillFocusWindow(window);
  if (GetActiveWindow() != window) {
    aura::client::GetFocusClient(window)->FocusWindow(
        FindFocusableWindowFor(window), event);
  }
}

}  // namespace internal
}  // namespace ash
