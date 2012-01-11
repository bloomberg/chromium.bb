// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/modal_container_layout_manager.h"

#include "ash/shell.h"
#include "ash/wm/modality_event_filter.h"
#include "ash/wm/window_util.h"
#include "base/bind.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/event.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/compositor/layer.h"
#include "ui/gfx/compositor/layer_animator.h"
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
    canvas->FillRect(SK_ColorBLACK, GetLocalBounds());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenView);
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ModalContainerLayoutManager, public:

ModalContainerLayoutManager::ModalContainerLayoutManager(
    aura::Window* container)
    : container_(container),
      modal_screen_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          modality_filter_(new ModalityEventFilter(container, this))) {
}

ModalContainerLayoutManager::~ModalContainerLayoutManager() {
}

////////////////////////////////////////////////////////////////////////////////
// ModalContainerLayoutManager, aura::LayoutManager implementation:

void ModalContainerLayoutManager::OnWindowResized() {
  if (modal_screen_) {
    modal_screen_->SetBounds(gfx::Rect(0, 0, container_->bounds().width(),
                                       container_->bounds().height()));
  }
}

void ModalContainerLayoutManager::OnWindowAddedToLayout(
    aura::Window* child) {
  child->AddObserver(this);
  if (child->GetIntProperty(aura::client::kModalKey))
    AddModalWindow(child);
}

void ModalContainerLayoutManager::OnWillRemoveWindowFromLayout(
    aura::Window* child) {
  child->RemoveObserver(this);
  if (child->GetIntProperty(aura::client::kModalKey))
    RemoveModalWindow(child);
}

void ModalContainerLayoutManager::OnChildWindowVisibilityChanged(
    aura::Window* child,
    bool visible) {
}

void ModalContainerLayoutManager::SetChildBounds(
    aura::Window* child,
    const gfx::Rect& requested_bounds) {
  SetChildBoundsDirect(child, requested_bounds);
}

////////////////////////////////////////////////////////////////////////////////
// ModalContainerLayoutManager, aura::WindowObserver implementation:

void ModalContainerLayoutManager::OnWindowPropertyChanged(aura::Window* window,
                                                          const char* key,
                                                          void* old) {
  if (key != aura::client::kModalKey)
    return;

  if (window->GetIntProperty(aura::client::kModalKey)) {
    AddModalWindow(window);
  } else if (static_cast<int>(reinterpret_cast<intptr_t>(old))) {
    RemoveModalWindow(window);
  }
}

////////////////////////////////////////////////////////////////////////////////
// ModalContainerLayoutManager, ui::LayerAnimationObserver implementation:

void ModalContainerLayoutManager::OnLayerAnimationEnded(
    const ui::LayerAnimationSequence* sequence) {
  if (modal_screen_ && !modal_screen_->GetNativeView()->layer()->ShouldDraw())
    DestroyModalScreen();
}

void ModalContainerLayoutManager::OnLayerAnimationAborted(
    const ui::LayerAnimationSequence* sequence) {
}

void ModalContainerLayoutManager::OnLayerAnimationScheduled(
    const ui::LayerAnimationSequence* sequence) {
}

////////////////////////////////////////////////////////////////////////////////
// ModalContainerLayoutManager, ModalityEventFilter::Delegate implementation:

bool ModalContainerLayoutManager::CanWindowReceiveEvents(
    aura::Window* window) {
  return GetActivatableWindow(window) == modal_window();
}

////////////////////////////////////////////////////////////////////////////////
// ModalContainerLayoutManager, private:

void ModalContainerLayoutManager::AddModalWindow(aura::Window* window) {
  modal_windows_.push_back(window);
  CreateModalScreen();
}

void ModalContainerLayoutManager::RemoveModalWindow(aura::Window* window) {
  aura::Window::Windows::iterator it =
      std::find(modal_windows_.begin(), modal_windows_.end(), window);
  if (it != modal_windows_.end())
    modal_windows_.erase(it);

  if (modal_windows_.empty())
    HideModalScreen();
  else
    ash::ActivateWindow(modal_window());
}

void ModalContainerLayoutManager::CreateModalScreen() {
  if (modal_screen_)
    return;
  modal_screen_ = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_CONTROL);
  params.parent = container_;
  params.bounds = gfx::Rect(0, 0, container_->bounds().width(),
                            container_->bounds().height());
  modal_screen_->Init(params);
  modal_screen_->GetNativeView()->SetName(
      "ModalContainerLayoutManager.ModalScreen");
  modal_screen_->SetContentsView(new ScreenView);
  modal_screen_->GetNativeView()->layer()->SetOpacity(0.0f);
  modal_screen_->GetNativeView()->layer()->GetAnimator()->AddObserver(this);

  Shell::GetInstance()->AddRootWindowEventFilter(modality_filter_.get());

  ui::LayerAnimator::ScopedSettings settings(
      modal_screen_->GetNativeView()->layer()->GetAnimator());
  modal_screen_->Show();
  modal_screen_->GetNativeView()->layer()->SetOpacity(0.5f);
  container_->StackChildAtTop(modal_screen_->GetNativeView());
}

void ModalContainerLayoutManager::DestroyModalScreen() {
  modal_screen_->GetNativeView()->layer()->GetAnimator()->RemoveObserver(this);
  modal_screen_->Close();
  modal_screen_ = NULL;
}

void ModalContainerLayoutManager::HideModalScreen() {
  Shell::GetInstance()->RemoveRootWindowEventFilter(modality_filter_.get());
  ui::LayerAnimator::ScopedSettings settings(
      modal_screen_->GetNativeView()->layer()->GetAnimator());
  modal_screen_->GetNativeView()->layer()->SetOpacity(0.0f);
}

}  // namespace internal
}  // namespace ash
