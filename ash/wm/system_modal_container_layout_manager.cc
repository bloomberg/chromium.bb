// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/system_modal_container_layout_manager.h"

#include <cmath>

#include "ash/session/session_state_delegate.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/dim_window.h"
#include "ash/wm/window_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_property.h"
#include "ui/compositor/layer.h"
#include "ui/keyboard/keyboard_controller.h"

namespace ash {
namespace {

// The center point of the window can diverge this much from the center point
// If this is set to true, the window will get centered.
DEFINE_WINDOW_PROPERTY_KEY(bool, kCenteredKey, false);

// The center point of the window can diverge this much from the center point
// of the container to be kept centered upon resizing operations.
const int kCenterPixelDelta = 32;
}

////////////////////////////////////////////////////////////////////////////////
// SystemModalContainerLayoutManager, public:

SystemModalContainerLayoutManager::SystemModalContainerLayoutManager(
    aura::Window* container)
    : SnapToPixelLayoutManager(container),
      container_(container),
      modal_background_(nullptr) {}

SystemModalContainerLayoutManager::~SystemModalContainerLayoutManager() {
}

////////////////////////////////////////////////////////////////////////////////
// SystemModalContainerLayoutManager, aura::LayoutManager implementation:

void SystemModalContainerLayoutManager::OnWindowResized() {
  PositionDialogsAfterWorkAreaResize();
}

void SystemModalContainerLayoutManager::OnWindowAddedToLayout(
    aura::Window* child) {
  DCHECK(child == modal_background_ ||
         child->type() == ui::wm::WINDOW_TYPE_NORMAL ||
         child->type() == ui::wm::WINDOW_TYPE_POPUP);
  DCHECK(
      container_->id() != kShellWindowId_LockSystemModalContainer ||
      Shell::GetInstance()->session_state_delegate()->IsUserSessionBlocked());

  child->AddObserver(this);
  if (child->GetProperty(aura::client::kModalKey) != ui::MODAL_TYPE_NONE)
    AddModalWindow(child);
}

void SystemModalContainerLayoutManager::OnWillRemoveWindowFromLayout(
    aura::Window* child) {
  child->RemoveObserver(this);
  if (child->GetProperty(aura::client::kModalKey) != ui::MODAL_TYPE_NONE)
    RemoveModalWindow(child);
}

void SystemModalContainerLayoutManager::SetChildBounds(
    aura::Window* child,
    const gfx::Rect& requested_bounds) {
  SnapToPixelLayoutManager::SetChildBounds(child, requested_bounds);
  child->SetProperty(kCenteredKey, DialogIsCentered(requested_bounds));
}

////////////////////////////////////////////////////////////////////////////////
// SystemModalContainerLayoutManager, aura::WindowObserver implementation:

void SystemModalContainerLayoutManager::OnWindowPropertyChanged(
    aura::Window* window,
    const void* key,
    intptr_t old) {
  if (key != aura::client::kModalKey)
    return;

  if (window->GetProperty(aura::client::kModalKey) != ui::MODAL_TYPE_NONE) {
    AddModalWindow(window);
  } else if (static_cast<ui::ModalType>(old) != ui::MODAL_TYPE_NONE) {
    RemoveModalWindow(window);
    Shell::GetInstance()->OnModalWindowRemoved(window);
  }
}

void SystemModalContainerLayoutManager::OnWindowDestroying(
    aura::Window* window) {
  if (modal_background_ == window) {
    if (keyboard::KeyboardController::GetInstance())
      keyboard::KeyboardController::GetInstance()->RemoveObserver(this);
    modal_background_ = nullptr;
  }
}

////////////////////////////////////////////////////////////////////////////////
// SystemModalContainerLayoutManager, Keyboard::KeybaordControllerObserver
// implementation:

void SystemModalContainerLayoutManager::OnKeyboardBoundsChanging(
    const gfx::Rect& new_bounds) {
  PositionDialogsAfterWorkAreaResize();
}

bool SystemModalContainerLayoutManager::CanWindowReceiveEvents(
    aura::Window* window) {
  // We could get when we're at lock screen and there is modal window at
  // system modal window layer which added event filter.
  // Now this lock modal windows layer layout manager should not block events
  // for windows at lock layer.
  // See SystemModalContainerLayoutManagerTest.EventFocusContainers and
  // http://crbug.com/157469
  if (modal_windows_.empty())
    return true;
  // This container can not handle events if the screen is locked and it is not
  // above the lock screen layer (crbug.com/110920).
  if (Shell::GetInstance()->session_state_delegate()->IsUserSessionBlocked() &&
      container_->id() < ash::kShellWindowId_LockScreenContainer)
    return true;
  return wm::GetActivatableWindow(window) == modal_window();
}

bool SystemModalContainerLayoutManager::ActivateNextModalWindow() {
  if (modal_windows_.empty())
    return false;
  wm::ActivateWindow(modal_window());
  return true;
}

void SystemModalContainerLayoutManager::CreateModalBackground() {
  if (!modal_background_) {
    modal_background_ = new DimWindow(container_);
    modal_background_->SetName(
        "SystemModalContainerLayoutManager.ModalBackground");
    // There isn't always a keyboard controller.
    if (keyboard::KeyboardController::GetInstance())
      keyboard::KeyboardController::GetInstance()->AddObserver(this);
  }
  modal_background_->Show();
}

void SystemModalContainerLayoutManager::DestroyModalBackground() {
  // modal_background_ can be NULL when a root window is shutting down.
  if (modal_background_) {
    if (keyboard::KeyboardController::GetInstance())
      keyboard::KeyboardController::GetInstance()->RemoveObserver(this);
    modal_background_->Hide();
    // Explicitly delete instead of using scoped_ptr as the owner of the
    // window is its parent.
    delete modal_background_;
    modal_background_ = nullptr;
  }
}

// static
bool SystemModalContainerLayoutManager::IsModalBackground(
    aura::Window* window) {
  int id = window->parent()->id();
  if (id != kShellWindowId_SystemModalContainer &&
      id != kShellWindowId_LockSystemModalContainer)
    return false;
  SystemModalContainerLayoutManager* layout_manager =
      static_cast<SystemModalContainerLayoutManager*>(
          window->parent()->layout_manager());
  return layout_manager->modal_background_ == window;
}

////////////////////////////////////////////////////////////////////////////////
// SystemModalContainerLayoutManager, private:

void SystemModalContainerLayoutManager::AddModalWindow(aura::Window* window) {
  if (modal_windows_.empty()) {
    aura::Window* capture_window = aura::client::GetCaptureWindow(container_);
    if (capture_window)
      capture_window->ReleaseCapture();
  }
  modal_windows_.push_back(window);
  Shell::GetInstance()->CreateModalBackground(window);
  window->parent()->StackChildAtTop(window);

  gfx::Rect target_bounds = window->bounds();
  target_bounds.AdjustToFit(GetUsableDialogArea());
  window->SetBounds(target_bounds);
}

void SystemModalContainerLayoutManager::RemoveModalWindow(
    aura::Window* window) {
  aura::Window::Windows::iterator it =
      std::find(modal_windows_.begin(), modal_windows_.end(), window);
  if (it != modal_windows_.end())
    modal_windows_.erase(it);
}

void SystemModalContainerLayoutManager::PositionDialogsAfterWorkAreaResize() {
  if (!modal_windows_.empty()) {
    for (aura::Window::Windows::iterator it = modal_windows_.begin();
         it != modal_windows_.end(); ++it) {
      (*it)->SetBounds(GetCenteredAndOrFittedBounds(*it));
    }
  }
}

gfx::Rect SystemModalContainerLayoutManager::GetUsableDialogArea() {
  // Instead of resizing the system modal container, we move only the modal
  // windows. This way we avoid flashing lines upon resize animation and if the
  // keyboard will not fill left to right, the background is still covered.
  gfx::Rect valid_bounds = container_->bounds();
  keyboard::KeyboardController* keyboard_controller =
      keyboard::KeyboardController::GetInstance();
  if (keyboard_controller) {
    gfx::Rect bounds = keyboard_controller->current_keyboard_bounds();
    if (!bounds.IsEmpty()) {
      valid_bounds.set_height(std::max(
          0, valid_bounds.height() - bounds.height()));
    }
  }
  return valid_bounds;
}

gfx::Rect SystemModalContainerLayoutManager::GetCenteredAndOrFittedBounds(
    const aura::Window* window) {
  gfx::Rect target_bounds;
  gfx::Rect usable_area = GetUsableDialogArea();
  if (window->GetProperty(kCenteredKey)) {
    // Keep the dialog centered if it was centered before.
    target_bounds = usable_area;
    target_bounds.ClampToCenteredSize(window->bounds().size());
  } else {
    // Keep the dialog within the usable area.
    target_bounds = window->bounds();
    target_bounds.AdjustToFit(usable_area);
  }
  if (usable_area != container_->bounds()) {
    // Don't clamp the dialog for the keyboard. Keep the size as it is but make
    // sure that the top remains visible.
    // TODO(skuhne): M37 should add over scroll functionality to address this.
    target_bounds.set_size(window->bounds().size());
  }
  return target_bounds;
}

bool SystemModalContainerLayoutManager::DialogIsCentered(
    const gfx::Rect& window_bounds) {
  gfx::Point window_center = window_bounds.CenterPoint();
  gfx::Point container_center = GetUsableDialogArea().CenterPoint();
  return
      std::abs(window_center.x() - container_center.x()) < kCenterPixelDelta &&
      std::abs(window_center.y() - container_center.y()) < kCenterPixelDelta;
}

}  // namespace ash
