// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/system_modal_container_layout_manager.h"

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/system_modal_container_event_filter.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/window_util.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/root_window.h"
#include "ui/views/corewm/compound_event_filter.h"
#include "ui/aura/window.h"
#include "ui/base/events/event.h"
#include "ui/base/ui_base_switches.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/screen.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace internal {

////////////////////////////////////////////////////////////////////////////////
// SystemModalContainerLayoutManager, public:

SystemModalContainerLayoutManager::SystemModalContainerLayoutManager(
    aura::Window* container)
    : container_(container),
      modal_background_(NULL) {
}

SystemModalContainerLayoutManager::~SystemModalContainerLayoutManager() {
}

////////////////////////////////////////////////////////////////////////////////
// SystemModalContainerLayoutManager, aura::LayoutManager implementation:

void SystemModalContainerLayoutManager::OnWindowResized() {
  if (modal_background_) {
    // Note: we have to set the entire bounds with the screen offset.
    modal_background_->SetBounds(
        Shell::GetScreen()->GetDisplayNearestWindow(container_).bounds());
  }
  if (!modal_windows_.empty()) {
    aura::Window::Windows::iterator it = modal_windows_.begin();
    for (it = modal_windows_.begin(); it != modal_windows_.end(); ++it) {
      gfx::Rect bounds = (*it)->bounds();
      bounds.AdjustToFit(container_->bounds());
      (*it)->SetBounds(bounds);
    }
  }
}

void SystemModalContainerLayoutManager::OnWindowAddedToLayout(
    aura::Window* child) {
  DCHECK((modal_background_ && child == modal_background_->GetNativeView()) ||
         child->type() == aura::client::WINDOW_TYPE_NORMAL ||
         child->type() == aura::client::WINDOW_TYPE_POPUP);
  DCHECK(
      container_->id() != internal::kShellWindowId_LockSystemModalContainer ||
      Shell::GetInstance()->delegate()->IsScreenLocked() ||
      !Shell::GetInstance()->delegate()->IsSessionStarted());

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

void SystemModalContainerLayoutManager::OnWindowRemovedFromLayout(
    aura::Window* child) {
}

void SystemModalContainerLayoutManager::OnChildWindowVisibilityChanged(
    aura::Window* child,
    bool visible) {
}

void SystemModalContainerLayoutManager::SetChildBounds(
    aura::Window* child,
    const gfx::Rect& requested_bounds) {
  SetChildBoundsDirect(child, requested_bounds);
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
  if (modal_background_ && modal_background_->GetNativeView() == window)
    modal_background_ = NULL;
}


////////////////////////////////////////////////////////////////////////////////
// SystemModalContainerLayoutManager,
//     SystemModalContainerEventFilter::Delegate implementation:

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
  if (ash::Shell::GetInstance()->IsScreenLocked() &&
      container_->id() < ash::internal::kShellWindowId_LockScreenContainer)
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
    modal_background_ = new views::Widget;
    views::Widget::InitParams params(views::Widget::InitParams::TYPE_CONTROL);
    params.parent = container_;
    params.bounds = Shell::GetScreen()->GetDisplayNearestWindow(
        container_).bounds();
    modal_background_->Init(params);
    modal_background_->GetNativeView()->SetName(
        "SystemModalContainerLayoutManager.ModalBackground");
    views::View* contents_view = new views::View();
    contents_view->set_background(views::Background::CreateSolidBackground(
        CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kEnableNewDialogStyle) ? SK_ColorWHITE : SK_ColorBLACK));
    modal_background_->SetContentsView(contents_view);
    modal_background_->GetNativeView()->layer()->SetOpacity(0.0f);
  }

  ui::ScopedLayerAnimationSettings settings(
      modal_background_->GetNativeView()->layer()->GetAnimator());
  modal_background_->Show();
  modal_background_->GetNativeView()->layer()->SetOpacity(0.5f);
  container_->StackChildAtTop(modal_background_->GetNativeView());
}

void SystemModalContainerLayoutManager::DestroyModalBackground() {
  // modal_background_ can be NULL when a root window is shutting down
  // and OnWindowDestroying is called first.
  if (modal_background_) {
    ui::ScopedLayerAnimationSettings settings(
        modal_background_->GetNativeView()->layer()->GetAnimator());
    modal_background_->Close();
    settings.AddObserver(views::corewm::CreateHidingWindowAnimationObserver(
        modal_background_->GetNativeView()));
    modal_background_->GetNativeView()->layer()->SetOpacity(0.0f);
    modal_background_ = NULL;
  }
}

// static
bool SystemModalContainerLayoutManager::IsModalBackground(
    aura::Window* window) {
  int id = window->parent()->id();
  if (id != internal::kShellWindowId_SystemModalContainer &&
      id != internal::kShellWindowId_LockSystemModalContainer)
    return false;
  SystemModalContainerLayoutManager* layout_manager =
      static_cast<SystemModalContainerLayoutManager*>(
          window->parent()->layout_manager());
  return layout_manager->modal_background_ &&
      layout_manager->modal_background_->GetNativeWindow() == window;
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
}

void SystemModalContainerLayoutManager::RemoveModalWindow(
    aura::Window* window) {
  aura::Window::Windows::iterator it =
      std::find(modal_windows_.begin(), modal_windows_.end(), window);
  if (it != modal_windows_.end())
    modal_windows_.erase(it);
}

}  // namespace internal
}  // namespace ash
