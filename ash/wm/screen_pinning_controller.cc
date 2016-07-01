// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/screen_pinning_controller.h"

#include <algorithm>
#include <vector>

#include "ash/aura/wm_window_aura.h"
#include "ash/common/shell_window_ids.h"
#include "ash/common/wm/window_state.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/common/wm_window_observer.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/shell.h"
#include "ash/wm/dim_window.h"
#include "base/auto_reset.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "ui/aura/window_observer.h"

namespace ash {
namespace {

WmWindow* CreateDimWindow(WmWindow* container) {
  DimWindow* dim_window = new DimWindow(WmWindowAura::GetAuraWindow(container));
  dim_window->SetDimOpacity(1);  // Set whole black.
  WmWindow* result = WmWindowAura::Get(dim_window);
  result->SetFullscreen();
  result->Show();
  return result;
}

// Returns a list of WmWindows corresponding to SystemModalContainers,
// except ones whose root is shared with |pinned_window|.
std::vector<WmWindow*> GetSystemModalWindowsExceptPinned(
    WmWindow* pinned_window) {
  WmWindow* pinned_root = pinned_window->GetRootWindow();

  std::vector<WmWindow*> result;
  for (WmWindow* system_modal :
       WmWindowAura::FromAuraWindows(Shell::GetContainersFromAllRootWindows(
           kShellWindowId_SystemModalContainer, nullptr))) {
    if (system_modal->GetRootWindow() == pinned_root)
      continue;
    result.push_back(system_modal);
  }
  return result;
}

void AddObserverToChildren(WmWindow* container,
                           aura::WindowObserver* observer) {
  for (WmWindow* child : container->GetChildren()) {
    WmWindowAura::GetAuraWindow(child)->AddObserver(observer);
  }
}

void RemoveObserverFromChildren(WmWindow* container,
                                aura::WindowObserver* observer) {
  for (WmWindow* child : container->GetChildren()) {
    WmWindowAura::GetAuraWindow(child)->RemoveObserver(observer);
  }
}

}  // namespace

// Adapter to fire OnPinnedContainerWindowStackingChanged().
class ScreenPinningController::PinnedContainerChildWindowObserver
    : public aura::WindowObserver {
 public:
  explicit PinnedContainerChildWindowObserver(
      ScreenPinningController* controller)
      : controller_(controller) {}

  void OnWindowStackingChanged(aura::Window* window) override {
    controller_->OnPinnedContainerWindowStackingChanged(
        WmWindowAura::Get(window));
  }

 private:
  ScreenPinningController* controller_;
  DISALLOW_COPY_AND_ASSIGN(PinnedContainerChildWindowObserver);
};

// Adapter to translate OnWindowAdded/OnWillRemoveWindow for the container
// containing the pinned window, to the corresponding controller's methods.
class ScreenPinningController::PinnedContainerWindowObserver
    : public aura::WindowObserver {
 public:
  explicit PinnedContainerWindowObserver(ScreenPinningController* controller)
      : controller_(controller) {}

  void OnWindowAdded(aura::Window* new_window) override {
    controller_->OnWindowAddedToPinnedContainer(WmWindowAura::Get(new_window));
  }
  void OnWillRemoveWindow(aura::Window* window) override {
    controller_->OnWillRemoveWindowFromPinnedContainer(
        WmWindowAura::Get(window));
  }
  void OnWindowDestroying(aura::Window* window) override {
    // Just in case. There is nothing we can do here.
    window->RemoveObserver(this);
  }

 private:
  ScreenPinningController* controller_;
  DISALLOW_COPY_AND_ASSIGN(PinnedContainerWindowObserver);
};

// Adapter to fire OnSystemModalContainerWindowStackingChanged().
class ScreenPinningController::SystemModalContainerChildWindowObserver
    : public aura::WindowObserver {
 public:
  explicit SystemModalContainerChildWindowObserver(
      ScreenPinningController* controller)
      : controller_(controller) {}

  void OnWindowStackingChanged(aura::Window* window) override {
    controller_->OnSystemModalContainerWindowStackingChanged(
        WmWindowAura::Get(window));
  }

 private:
  ScreenPinningController* controller_;
  DISALLOW_COPY_AND_ASSIGN(SystemModalContainerChildWindowObserver);
};

// Adapter to translate OnWindowAdded/OnWillRemoveWindow for the
// SystemModalContainer to the corresponding controller's methods.
class ScreenPinningController::SystemModalContainerWindowObserver
    : public aura::WindowObserver {
 public:
  explicit SystemModalContainerWindowObserver(
      ScreenPinningController* controller)
      : controller_(controller) {}

  void OnWindowAdded(aura::Window* new_window) override {
    controller_->OnWindowAddedToSystemModalContainer(
        WmWindowAura::Get(new_window));
  }
  void OnWillRemoveWindow(aura::Window* window) override {
    controller_->OnWillRemoveWindowFromSystemModalContainer(
        WmWindowAura::Get(window));
  }
  void OnWindowDestroying(aura::Window* window) override {
    // Just in case. There is nothing we can do here.
    window->RemoveObserver(this);
  }

 private:
  ScreenPinningController* controller_;
  DISALLOW_COPY_AND_ASSIGN(SystemModalContainerWindowObserver);
};

// Tracks DimWindow's life.
class ScreenPinningController::DimWindowObserver : public aura::WindowObserver {
 public:
  explicit DimWindowObserver(ScreenPinningController* controller)
      : controller_(controller) {}

  void OnWindowParentChanged(aura::Window* window,
                             aura::Window* parent) override {
    // In case of parent change, there is nothing we can do for that change.
    // Also, it is unsafe to delete the moving window here, because it could
    // cause SEGV. Instead, we just hide it.
    // Note that, the window is still tracked by the ScreenPinningController,
    // so that it should be deleted on "unpin", at least.
    window->Hide();
  }

  void OnWindowDestroying(aura::Window* window) override {
    controller_->OnDimWindowDestroying(WmWindowAura::Get(window));
  }

 private:
  ScreenPinningController* controller_;
  DISALLOW_COPY_AND_ASSIGN(DimWindowObserver);
};

ScreenPinningController::ScreenPinningController(
    WindowTreeHostManager* window_tree_host_manager)
    : window_tree_host_manager_(window_tree_host_manager),
      pinned_container_window_observer_(
          new PinnedContainerWindowObserver(this)),
      pinned_container_child_window_observer_(
          new PinnedContainerChildWindowObserver(this)),
      system_modal_container_window_observer_(
          new SystemModalContainerWindowObserver(this)),
      system_modal_container_child_window_observer_(
          new SystemModalContainerChildWindowObserver(this)),
      dim_window_observer_(new DimWindowObserver(this)) {
  window_tree_host_manager_->AddObserver(this);
}

ScreenPinningController::~ScreenPinningController() {
  window_tree_host_manager_->RemoveObserver(this);
}

bool ScreenPinningController::IsPinned() const {
  return pinned_window_ != nullptr;
}

void ScreenPinningController::SetPinnedWindow(WmWindow* pinned_window) {
  if (pinned_window->GetWindowState()->IsPinned()) {
    if (pinned_window_) {
      LOG(DFATAL) << "Pinned mode is enabled, while it is already in "
                  << "the pinned mode";
      return;
    }

    WmWindow* container = pinned_window->GetParent();
    std::vector<WmWindow*> system_modal_containers =
        GetSystemModalWindowsExceptPinned(pinned_window);

    // Set up the container which has the pinned window.
    pinned_window_ = pinned_window;
    background_window_ = CreateDimWindow(container);
    container->StackChildAtTop(pinned_window);
    container->StackChildBelow(background_window_, pinned_window);

    // Set the dim windows to the system containers, other than the one which
    // the root window of the pinned window holds.
    for (WmWindow* system_modal : system_modal_containers) {
      WmWindow* dim_window = CreateDimWindow(system_modal);
      system_modal->StackChildAtBottom(dim_window);
      dim_windows_.push_back(dim_window);
      WmWindowAura::GetAuraWindow(dim_window)
          ->AddObserver(dim_window_observer_.get());
    }

    // Set observers.
    WmWindowAura::GetAuraWindow(container)->AddObserver(
        pinned_container_window_observer_.get());
    AddObserverToChildren(container,
                          pinned_container_child_window_observer_.get());
    for (WmWindow* system_modal : system_modal_containers) {
      WmWindowAura::GetAuraWindow(system_modal)
          ->AddObserver(system_modal_container_window_observer_.get());
      AddObserverToChildren(
          system_modal, system_modal_container_child_window_observer_.get());
    }
  } else {
    if (pinned_window != pinned_window_) {
      LOG(DFATAL) << "Pinned mode is being disabled, but for the different "
                  << "target window.";
      return;
    }

    WmWindow* container = pinned_window->GetParent();
    std::vector<WmWindow*> system_modal_containers =
        GetSystemModalWindowsExceptPinned(pinned_window_);

    // Unset observers.
    for (WmWindow* system_modal :
         GetSystemModalWindowsExceptPinned(pinned_window_)) {
      RemoveObserverFromChildren(
          system_modal, system_modal_container_child_window_observer_.get());
      WmWindowAura::GetAuraWindow(system_modal)
          ->RemoveObserver(system_modal_container_window_observer_.get());
    }
    RemoveObserverFromChildren(container,
                               pinned_container_child_window_observer_.get());
    WmWindowAura::GetAuraWindow(container)->RemoveObserver(
        pinned_container_window_observer_.get());

    // Delete the dim windows. This works, but do not use RAII, because
    // the window is owned by the parent.
    // Note: dim_windows_ will be updated during the deletion. So, copy is
    // needed.
    for (WmWindow* dim_window : std::vector<WmWindow*>(dim_windows_)) {
      delete WmWindowAura::GetAuraWindow(dim_window);
    }
    DCHECK(dim_windows_.empty());
    delete WmWindowAura::GetAuraWindow(background_window_);
    background_window_ = nullptr;

    pinned_window_ = nullptr;
  }

  WmShell::Get()->NotifyPinnedStateChanged(pinned_window);
}

void ScreenPinningController::OnWindowAddedToPinnedContainer(
    WmWindow* new_window) {
  KeepPinnedWindowOnTop();
  WmWindowAura::GetAuraWindow(new_window)
      ->AddObserver(pinned_container_child_window_observer_.get());
}

void ScreenPinningController::OnWillRemoveWindowFromPinnedContainer(
    WmWindow* window) {
  WmWindowAura::GetAuraWindow(window)->RemoveObserver(
      pinned_container_child_window_observer_.get());
  if (window == pinned_window_) {
    pinned_window_->GetWindowState()->Restore();
    return;
  }
  if (window == background_window_) {
    background_window_ = nullptr;
    return;
  }
}

void ScreenPinningController::OnPinnedContainerWindowStackingChanged(
    WmWindow* window) {
  KeepPinnedWindowOnTop();
}

void ScreenPinningController::OnWindowAddedToSystemModalContainer(
    WmWindow* new_window) {
  KeepDimWindowAtBottom(new_window->GetParent());
  WmWindowAura::GetAuraWindow(new_window)
      ->AddObserver(system_modal_container_child_window_observer_.get());
}

void ScreenPinningController::OnWillRemoveWindowFromSystemModalContainer(
    WmWindow* window) {
  WmWindowAura::GetAuraWindow(window)->RemoveObserver(
      system_modal_container_child_window_observer_.get());
}

void ScreenPinningController::OnSystemModalContainerWindowStackingChanged(
    WmWindow* window) {
  KeepDimWindowAtBottom(window->GetParent());
}

void ScreenPinningController::OnDimWindowDestroying(WmWindow* window) {
  dim_windows_.erase(
      std::find(dim_windows_.begin(), dim_windows_.end(), window));
}

void ScreenPinningController::OnDisplayConfigurationChanged() {
  // Note: this is called on display attached or detached.
  if (!IsPinned())
    return;

  // On display detaching, all necessary windows are transfered to the
  // primary display's tree, and called this.
  // So, delete the dim windows which are not a part of target system modal
  // container.
  // On display attaching, the new system modal container does not have the
  // dim window. So create it.

  // First, delete unnecessary dim windows.
  // The delete will update dim_windows_, so create the copy is needed.
  for (WmWindow* dim_window : std::vector<WmWindow*>(dim_windows_)) {
    if (!dim_window->GetTargetVisibility())
      delete WmWindowAura::GetAuraWindow(dim_window);
  }

  // Then, create missing dim_windows.
  std::vector<WmWindow*> system_modal_containers =
      GetSystemModalWindowsExceptPinned(pinned_window_);
  for (WmWindow* system_modal : system_modal_containers) {
    const std::vector<WmWindow*> children = system_modal->GetChildren();
    if (!children.empty() && ContainsValue(dim_windows_, children[0])) {
      // The system modal dialog has the dim window.
      continue;
    }

    // This is the new system modal dialog.
    WmWindow* dim_window = CreateDimWindow(system_modal);
    system_modal->StackChildAtBottom(dim_window);
    dim_windows_.push_back(dim_window);
    WmWindowAura::GetAuraWindow(dim_window)
        ->AddObserver(dim_window_observer_.get());

    // Set observers to the tree.
    WmWindowAura::GetAuraWindow(system_modal)
        ->AddObserver(system_modal_container_window_observer_.get());
    AddObserverToChildren(system_modal,
                          system_modal_container_child_window_observer_.get());
  }
}

void ScreenPinningController::KeepPinnedWindowOnTop() {
  if (in_restacking_)
    return;

  base::AutoReset<bool> auto_reset(&in_restacking_, true);
  WmWindow* container = pinned_window_->GetParent();
  container->StackChildAtTop(pinned_window_);
  container->StackChildBelow(background_window_, pinned_window_);
}

void ScreenPinningController::KeepDimWindowAtBottom(WmWindow* container) {
  if (in_restacking_)
    return;

  base::AutoReset<bool> auto_reset(&in_restacking_, true);
  for (WmWindow* dim_window : dim_windows_) {
    if (dim_window->GetParent() == container) {
      container->StackChildAtBottom(dim_window);
      break;
    }
  }
}

}  // namespace ash
