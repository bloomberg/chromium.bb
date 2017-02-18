// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/wm/system_modal_container_layout_manager.h"

#include <cmath>

#include "ash/common/session/session_state_delegate.h"
#include "ash/common/wm/window_dimmer.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/common/wm_window_property.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/keyboard/keyboard_controller.h"

namespace ash {
namespace {

// The center point of the window can diverge this much from the center point
// of the container to be kept centered upon resizing operations.
const int kCenterPixelDelta = 32;

ui::ModalType GetModalType(WmWindow* window) {
  return static_cast<ui::ModalType>(
      window->GetIntProperty(WmWindowProperty::MODAL_TYPE));
}

bool HasTransientAncestor(const WmWindow* window, const WmWindow* ancestor) {
  const WmWindow* transient_parent = window->GetTransientParent();
  if (transient_parent == ancestor)
    return true;
  return transient_parent ? HasTransientAncestor(transient_parent, ancestor)
                          : false;
}
}

////////////////////////////////////////////////////////////////////////////////
// SystemModalContainerLayoutManager, public:

SystemModalContainerLayoutManager::SystemModalContainerLayoutManager(
    WmWindow* container)
    : container_(container) {}

SystemModalContainerLayoutManager::~SystemModalContainerLayoutManager() {
  if (keyboard::KeyboardController::GetInstance())
    keyboard::KeyboardController::GetInstance()->RemoveObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// SystemModalContainerLayoutManager, WmLayoutManager implementation:

void SystemModalContainerLayoutManager::OnChildWindowVisibilityChanged(
    WmWindow* window,
    bool visible) {
  if (GetModalType(window) != ui::MODAL_TYPE_SYSTEM)
    return;

  if (window->IsVisible()) {
    DCHECK(!base::ContainsValue(modal_windows_, window));
    AddModalWindow(window);
  } else {
    if (RemoveModalWindow(window))
      WmShell::Get()->OnModalWindowRemoved(window);
  }
}

void SystemModalContainerLayoutManager::OnWindowResized() {
  PositionDialogsAfterWorkAreaResize();
}

void SystemModalContainerLayoutManager::OnWindowAddedToLayout(WmWindow* child) {
  DCHECK(child->GetType() == ui::wm::WINDOW_TYPE_NORMAL ||
         child->GetType() == ui::wm::WINDOW_TYPE_POPUP);
  // TODO(mash): IsUserSessionBlocked() depends on knowing the login state. We
  // need a non-stub version of SessionStateDelegate. crbug.com/648964
  if (!WmShell::Get()->IsRunningInMash()) {
    DCHECK(container_->GetShellWindowId() !=
               kShellWindowId_LockSystemModalContainer ||
           WmShell::Get()->GetSessionStateDelegate()->IsUserSessionBlocked());
  }
  // Since this is for SystemModal, there is no good reason to add windows
  // other than MODAL_TYPE_NONE or MODAL_TYPE_SYSTEM. DCHECK to avoid simple
  // mistake.
  DCHECK_NE(GetModalType(child), ui::MODAL_TYPE_CHILD);
  DCHECK_NE(GetModalType(child), ui::MODAL_TYPE_WINDOW);

  child->aura_window()->AddObserver(this);
  if (GetModalType(child) == ui::MODAL_TYPE_SYSTEM && child->IsVisible())
    AddModalWindow(child);
}

void SystemModalContainerLayoutManager::OnWillRemoveWindowFromLayout(
    WmWindow* child) {
  child->aura_window()->RemoveObserver(this);
  windows_to_center_.erase(child);
  if (GetModalType(child) == ui::MODAL_TYPE_SYSTEM)
    RemoveModalWindow(child);
}

void SystemModalContainerLayoutManager::SetChildBounds(
    WmWindow* child,
    const gfx::Rect& requested_bounds) {
  WmSnapToPixelLayoutManager::SetChildBounds(child, requested_bounds);
  if (IsBoundsCentered(requested_bounds))
    windows_to_center_.insert(child);
  else
    windows_to_center_.erase(child);
}

////////////////////////////////////////////////////////////////////////////////
// SystemModalContainerLayoutManager, aura::WindowObserver implementation:

void SystemModalContainerLayoutManager::OnWindowPropertyChanged(
    aura::Window* window,
    const void* key,
    intptr_t old) {
  if (key != aura::client::kModalKey || !window->IsVisible())
    return;

  WmWindow* wm_window = WmWindow::Get(window);
  if (window->GetProperty(aura::client::kModalKey) == ui::MODAL_TYPE_SYSTEM) {
    if (base::ContainsValue(modal_windows_, wm_window))
      return;
    AddModalWindow(wm_window);
  } else {
    if (RemoveModalWindow(wm_window))
      WmShell::Get()->OnModalWindowRemoved(wm_window);
  }
}

////////////////////////////////////////////////////////////////////////////////
// SystemModalContainerLayoutManager, Keyboard::KeybaordControllerObserver
// implementation:

void SystemModalContainerLayoutManager::OnKeyboardBoundsChanging(
    const gfx::Rect& new_bounds) {
  PositionDialogsAfterWorkAreaResize();
}

void SystemModalContainerLayoutManager::OnKeyboardClosed() {}

bool SystemModalContainerLayoutManager::IsPartOfActiveModalWindow(
    WmWindow* window) {
  return modal_window() &&
         (modal_window()->Contains(window) ||
          HasTransientAncestor(window->GetToplevelWindowForFocus(),
                               modal_window()));
}

bool SystemModalContainerLayoutManager::ActivateNextModalWindow() {
  if (modal_windows_.empty())
    return false;
  modal_window()->Activate();
  return true;
}

void SystemModalContainerLayoutManager::CreateModalBackground() {
  if (!window_dimmer_) {
    window_dimmer_ = base::MakeUnique<WindowDimmer>(container_);
    window_dimmer_->window()->SetName(
        "SystemModalContainerLayoutManager.ModalBackground");
    // There isn't always a keyboard controller.
    if (keyboard::KeyboardController::GetInstance())
      keyboard::KeyboardController::GetInstance()->AddObserver(this);
  }
  window_dimmer_->window()->Show();
}

void SystemModalContainerLayoutManager::DestroyModalBackground() {
  if (!window_dimmer_)
    return;

  if (keyboard::KeyboardController::GetInstance())
    keyboard::KeyboardController::GetInstance()->RemoveObserver(this);
  window_dimmer_.reset();
}

// static
bool SystemModalContainerLayoutManager::IsModalBackground(WmWindow* window) {
  int id = window->GetParent()->GetShellWindowId();
  if (id != kShellWindowId_SystemModalContainer &&
      id != kShellWindowId_LockSystemModalContainer)
    return false;
  SystemModalContainerLayoutManager* layout_manager =
      static_cast<SystemModalContainerLayoutManager*>(
          window->GetParent()->GetLayoutManager());
  return layout_manager->window_dimmer_ &&
         layout_manager->window_dimmer_->window() == window;
}

////////////////////////////////////////////////////////////////////////////////
// SystemModalContainerLayoutManager, private:

void SystemModalContainerLayoutManager::AddModalWindow(WmWindow* window) {
  if (modal_windows_.empty()) {
    WmWindow* capture_window = WmShell::Get()->GetCaptureWindow();
    if (capture_window)
      capture_window->ReleaseCapture();
  }
  DCHECK(window->IsVisible());
  DCHECK(!base::ContainsValue(modal_windows_, window));

  modal_windows_.push_back(window);
  WmShell::Get()->CreateModalBackground(window);
  window->GetParent()->StackChildAtTop(window);

  gfx::Rect target_bounds = window->GetBounds();
  target_bounds.AdjustToFit(GetUsableDialogArea());
  window->SetBounds(target_bounds);
}

bool SystemModalContainerLayoutManager::RemoveModalWindow(WmWindow* window) {
  auto it = std::find(modal_windows_.begin(), modal_windows_.end(), window);
  if (it == modal_windows_.end())
    return false;
  modal_windows_.erase(it);
  return true;
}

void SystemModalContainerLayoutManager::PositionDialogsAfterWorkAreaResize() {
  if (modal_windows_.empty())
    return;

  for (WmWindow* window : modal_windows_)
    window->SetBounds(GetCenteredAndOrFittedBounds(window));
}

gfx::Rect SystemModalContainerLayoutManager::GetUsableDialogArea() const {
  // Instead of resizing the system modal container, we move only the modal
  // windows. This way we avoid flashing lines upon resize animation and if the
  // keyboard will not fill left to right, the background is still covered.
  gfx::Rect valid_bounds = container_->GetBounds();
  keyboard::KeyboardController* keyboard_controller =
      keyboard::KeyboardController::GetInstance();
  if (keyboard_controller) {
    gfx::Rect bounds = keyboard_controller->current_keyboard_bounds();
    if (!bounds.IsEmpty()) {
      valid_bounds.set_height(
          std::max(0, valid_bounds.height() - bounds.height()));
    }
  }
  return valid_bounds;
}

gfx::Rect SystemModalContainerLayoutManager::GetCenteredAndOrFittedBounds(
    const WmWindow* window) {
  gfx::Rect target_bounds;
  gfx::Rect usable_area = GetUsableDialogArea();
  if (windows_to_center_.count(window) > 0) {
    // Keep the dialog centered if it was centered before.
    target_bounds = usable_area;
    target_bounds.ClampToCenteredSize(window->GetBounds().size());
  } else {
    // Keep the dialog within the usable area.
    target_bounds = window->GetBounds();
    target_bounds.AdjustToFit(usable_area);
  }
  if (usable_area != container_->GetBounds()) {
    // Don't clamp the dialog for the keyboard. Keep the size as it is but make
    // sure that the top remains visible.
    // TODO(skuhne): M37 should add over scroll functionality to address this.
    target_bounds.set_size(window->GetBounds().size());
  }
  return target_bounds;
}

bool SystemModalContainerLayoutManager::IsBoundsCentered(
    const gfx::Rect& bounds) const {
  gfx::Point window_center = bounds.CenterPoint();
  gfx::Point container_center = GetUsableDialogArea().CenterPoint();
  return std::abs(window_center.x() - container_center.x()) <
             kCenterPixelDelta &&
         std::abs(window_center.y() - container_center.y()) < kCenterPixelDelta;
}

}  // namespace ash
