// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/screen_pinning_controller.h"

#include <algorithm>
#include <vector>

#include "ash/common/wm/container_finder.h"
#include "ash/common/wm/window_dimmer.h"
#include "ash/common/wm/window_state.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/common/wm_window_user_data.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "base/auto_reset.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "ui/aura/window_observer.h"

namespace ash {
namespace {

// Returns a list of WmWindows corresponding to SystemModalContainers,
// except ones whose root is shared with |pinned_window|.
std::vector<WmWindow*> GetSystemModalWindowsExceptPinned(
    WmWindow* pinned_window) {
  WmWindow* pinned_root = pinned_window->GetRootWindow();

  std::vector<WmWindow*> result;
  for (WmWindow* system_modal : wm::GetContainersFromAllRootWindows(
           kShellWindowId_SystemModalContainer)) {
    if (system_modal->GetRootWindow() == pinned_root)
      continue;
    result.push_back(system_modal);
  }
  return result;
}

void AddObserverToChildren(WmWindow* container,
                           aura::WindowObserver* observer) {
  for (WmWindow* child : container->GetChildren()) {
    WmWindow::GetAuraWindow(child)->AddObserver(observer);
  }
}

void RemoveObserverFromChildren(WmWindow* container,
                                aura::WindowObserver* observer) {
  for (WmWindow* child : container->GetChildren()) {
    WmWindow::GetAuraWindow(child)->RemoveObserver(observer);
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
    controller_->OnPinnedContainerWindowStackingChanged(WmWindow::Get(window));
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
    controller_->OnWindowAddedToPinnedContainer(WmWindow::Get(new_window));
  }
  void OnWillRemoveWindow(aura::Window* window) override {
    controller_->OnWillRemoveWindowFromPinnedContainer(WmWindow::Get(window));
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
        WmWindow::Get(window));
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
    controller_->OnWindowAddedToSystemModalContainer(WmWindow::Get(new_window));
  }
  void OnWillRemoveWindow(aura::Window* window) override {
    controller_->OnWillRemoveWindowFromSystemModalContainer(
        WmWindow::Get(window));
  }
  void OnWindowDestroying(aura::Window* window) override {
    // Just in case. There is nothing we can do here.
    window->RemoveObserver(this);
  }

 private:
  ScreenPinningController* controller_;
  DISALLOW_COPY_AND_ASSIGN(SystemModalContainerWindowObserver);
};

ScreenPinningController::ScreenPinningController(
    WindowTreeHostManager* window_tree_host_manager)
    : window_dimmers_(base::MakeUnique<WmWindowUserData<WindowDimmer>>()),
      window_tree_host_manager_(window_tree_host_manager),
      pinned_container_window_observer_(
          base::MakeUnique<PinnedContainerWindowObserver>(this)),
      pinned_container_child_window_observer_(
          base::MakeUnique<PinnedContainerChildWindowObserver>(this)),
      system_modal_container_window_observer_(
          base::MakeUnique<SystemModalContainerWindowObserver>(this)),
      system_modal_container_child_window_observer_(
          base::MakeUnique<SystemModalContainerChildWindowObserver>(this)) {
  window_tree_host_manager_->AddObserver(this);
}

ScreenPinningController::~ScreenPinningController() {
  window_tree_host_manager_->RemoveObserver(this);
}

bool ScreenPinningController::IsPinned() const {
  return pinned_window_ != nullptr;
}

void ScreenPinningController::SetPinnedWindow(WmWindow* pinned_window) {
  window_dimmers_->clear();

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
    container->StackChildAtTop(pinned_window);
    container->StackChildBelow(CreateWindowDimmer(container), pinned_window);

    // Set the dim windows to the system containers, other than the one which
    // the root window of the pinned window holds.
    for (WmWindow* system_modal : system_modal_containers)
      system_modal->StackChildAtBottom(CreateWindowDimmer(system_modal));

    // Set observers.
    WmWindow::GetAuraWindow(container)->AddObserver(
        pinned_container_window_observer_.get());
    AddObserverToChildren(container,
                          pinned_container_child_window_observer_.get());
    for (WmWindow* system_modal : system_modal_containers) {
      WmWindow::GetAuraWindow(system_modal)
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
      WmWindow::GetAuraWindow(system_modal)
          ->RemoveObserver(system_modal_container_window_observer_.get());
    }
    RemoveObserverFromChildren(container,
                               pinned_container_child_window_observer_.get());
    WmWindow::GetAuraWindow(container)->RemoveObserver(
        pinned_container_window_observer_.get());

    pinned_window_ = nullptr;
  }

  WmShell::Get()->NotifyPinnedStateChanged(pinned_window);
}

void ScreenPinningController::OnWindowAddedToPinnedContainer(
    WmWindow* new_window) {
  KeepPinnedWindowOnTop();
  WmWindow::GetAuraWindow(new_window)
      ->AddObserver(pinned_container_child_window_observer_.get());
}

void ScreenPinningController::OnWillRemoveWindowFromPinnedContainer(
    WmWindow* window) {
  WmWindow::GetAuraWindow(window)->RemoveObserver(
      pinned_container_child_window_observer_.get());
  if (window == pinned_window_) {
    pinned_window_->GetWindowState()->Restore();
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
  WmWindow::GetAuraWindow(new_window)
      ->AddObserver(system_modal_container_child_window_observer_.get());
}

void ScreenPinningController::OnWillRemoveWindowFromSystemModalContainer(
    WmWindow* window) {
  WmWindow::GetAuraWindow(window)->RemoveObserver(
      system_modal_container_child_window_observer_.get());
}

void ScreenPinningController::OnSystemModalContainerWindowStackingChanged(
    WmWindow* window) {
  KeepDimWindowAtBottom(window->GetParent());
}

WmWindow* ScreenPinningController::CreateWindowDimmer(WmWindow* container) {
  std::unique_ptr<WindowDimmer> window_dimmer =
      base::MakeUnique<WindowDimmer>(container);
  window_dimmer->SetDimOpacity(1);  // Fully opaque.
  window_dimmer->window()->SetFullscreen(true);
  window_dimmer->window()->Show();
  WmWindow* window = window_dimmer->window();
  window_dimmers_->Set(container, std::move(window_dimmer));
  return window;
}

void ScreenPinningController::OnDisplayConfigurationChanged() {
  // Note: this is called on display attached or detached.
  if (!IsPinned())
    return;

  // On display detaching, all necessary windows are transfered to the
  // primary display's tree, and called this.
  // So, delete WindowDimmers which are not a part of target system modal
  // container.
  // On display attaching, the new system modal container does not have the
  // WindowDimmer. So create it.

  // First, delete unnecessary WindowDimmers.
  for (WmWindow* container : window_dimmers_->GetWindows()) {
    if (container != pinned_window_->GetParent() &&
        !window_dimmers_->Get(container)->window()->GetTargetVisibility()) {
      window_dimmers_->Set(container, nullptr);
    }
  }

  // Then, create missing WindowDimmers.
  std::vector<WmWindow*> system_modal_containers =
      GetSystemModalWindowsExceptPinned(pinned_window_);
  for (WmWindow* system_modal : system_modal_containers) {
    if (window_dimmers_->Get(system_modal)) {
      // |system_modal| already has a WindowDimmer.
      continue;
    }

    // This is the new system modal dialog.
    system_modal->StackChildAtBottom(CreateWindowDimmer(system_modal));

    // Set observers to the tree.
    WmWindow::GetAuraWindow(system_modal)
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
  WindowDimmer* pinned_window_dimmer = window_dimmers_->Get(container);
  if (pinned_window_dimmer && pinned_window_dimmer->window())
    container->StackChildBelow(pinned_window_dimmer->window(), pinned_window_);
}

void ScreenPinningController::KeepDimWindowAtBottom(WmWindow* container) {
  if (in_restacking_)
    return;

  WindowDimmer* window_dimmer = window_dimmers_->Get(container);
  if (window_dimmer) {
    base::AutoReset<bool> auto_reset(&in_restacking_, true);
    container->StackChildAtBottom(window_dimmer->window());
  }
}

}  // namespace ash
