// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/system_modal_container_layout_manager.h"

#include "ash/ash_switches.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/system_modal_container_event_filter.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/window_util.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/base/events/event.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace internal {

namespace {

class ScreenView : public views::View {
 public:
  ScreenView() {}
  virtual ~ScreenView() {}

  // Overridden from views::View:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    canvas->FillRect(GetLocalBounds(), GetOverlayColor());
  }

 private:
  SkColor GetOverlayColor() {
    if (CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kAuraGoogleDialogFrames)) {
      return SK_ColorWHITE;
    }
    return SK_ColorBLACK;
  }

  DISALLOW_COPY_AND_ASSIGN(ScreenView);
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// SystemModalContainerLayoutManager, public:

SystemModalContainerLayoutManager::SystemModalContainerLayoutManager(
    aura::Window* container)
    : container_(container),
      modal_screen_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(modality_filter_(
          new SystemModalContainerEventFilter(this))) {
}

SystemModalContainerLayoutManager::~SystemModalContainerLayoutManager() {
}

////////////////////////////////////////////////////////////////////////////////
// SystemModalContainerLayoutManager, aura::LayoutManager implementation:

void SystemModalContainerLayoutManager::OnWindowResized() {
  if (modal_screen_) {
    // Note: we have to set the entire bounds with the screen offset.
    modal_screen_->SetBounds(container_->bounds());
  }
  if (!modal_windows_.empty()) {
    aura::Window::Windows::iterator it = modal_windows_.begin();
    for (it = modal_windows_.begin(); it != modal_windows_.end(); ++it) {
      (*it)->SetBounds((*it)->bounds().AdjustToFit(container_->bounds()));
    }
  }
}

void SystemModalContainerLayoutManager::OnWindowAddedToLayout(
    aura::Window* child) {
  DCHECK((modal_screen_ && child == modal_screen_->GetNativeView()) ||
         child->type() == aura::client::WINDOW_TYPE_NORMAL ||
         child->type() == aura::client::WINDOW_TYPE_POPUP);
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
  }
}

void SystemModalContainerLayoutManager::OnWindowDestroying(
    aura::Window* window) {
  if (modal_screen_ && modal_screen_->GetNativeView() == window)
    modal_screen_ = NULL;
}


////////////////////////////////////////////////////////////////////////////////
// SystemModalContainerLayoutManager,
//     SystemModalContainerEventFilter::Delegate implementation:

bool SystemModalContainerLayoutManager::CanWindowReceiveEvents(
    aura::Window* window) {
  // This container can not handle events if the screen is locked and it is not
  // above the lock screen layer (crbug.com/110920).
  if (ash::Shell::GetInstance()->IsScreenLocked() &&
      container_->id() < ash::internal::kShellWindowId_LockScreenContainer)
    return true;
  return wm::GetActivatableWindow(window) == modal_window();
}

bool SystemModalContainerLayoutManager::IsModalScreen(
    aura::Window* window) {
  int id = window->parent()->id();
  return (id == internal::kShellWindowId_SystemModalContainer ||
          id == internal::kShellWindowId_LockSystemModalContainer) &&
      window->GetProperty(aura::client::kModalKey) == ui::MODAL_TYPE_NONE;
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
  CreateModalScreen();
}

void SystemModalContainerLayoutManager::RemoveModalWindow(
    aura::Window* window) {
  aura::Window::Windows::iterator it =
      std::find(modal_windows_.begin(), modal_windows_.end(), window);
  if (it != modal_windows_.end())
    modal_windows_.erase(it);

  if (modal_windows_.empty())
    DestroyModalScreen();
  else
    wm::ActivateWindow(modal_window());
}

void SystemModalContainerLayoutManager::CreateModalScreen() {
  if (!modal_screen_) {
    modal_screen_ = new views::Widget;
    views::Widget::InitParams params(views::Widget::InitParams::TYPE_CONTROL);
    params.parent = container_;
    params.bounds = gfx::Rect(0, 0, container_->bounds().width(),
        container_->bounds().height());
    modal_screen_->Init(params);
    modal_screen_->GetNativeView()->SetName(
        "SystemModalContainerLayoutManager.ModalScreen");
    modal_screen_->SetContentsView(new ScreenView);
    modal_screen_->GetNativeView()->layer()->SetOpacity(0.0f);

    Shell::GetInstance()->AddEnvEventFilter(modality_filter_.get());
  }

  ui::ScopedLayerAnimationSettings settings(
      modal_screen_->GetNativeView()->layer()->GetAnimator());
  modal_screen_->Show();
  modal_screen_->GetNativeView()->layer()->SetOpacity(0.5f);
  container_->StackChildAtTop(modal_screen_->GetNativeView());
}

void SystemModalContainerLayoutManager::DestroyModalScreen() {
  Shell::GetInstance()->RemoveEnvEventFilter(modality_filter_.get());
  // modal_screen_ can be NULL when a root window is shutting down
  // and OnWindowDestroying is called first.
  if (modal_screen_) {
    ui::ScopedLayerAnimationSettings settings(
        modal_screen_->GetNativeView()->layer()->GetAnimator());
    modal_screen_->Close();
    settings.AddObserver(
        CreateHidingWindowAnimationObserver(modal_screen_->GetNativeView()));
    modal_screen_->GetNativeView()->layer()->SetOpacity(0.0f);
    modal_screen_ = NULL;
  }
}

}  // namespace internal
}  // namespace ash
