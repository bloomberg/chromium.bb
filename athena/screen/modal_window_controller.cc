// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/screen/modal_window_controller.h"

#include "athena/screen/public/screen_manager.h"
#include "base/message_loop/message_loop.h"
#include "ui/aura/window.h"
#include "ui/aura/window_property.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/wm/core/window_animations.h"

DECLARE_WINDOW_PROPERTY_TYPE(athena::ModalWindowController*);

namespace athena {
namespace {

DEFINE_OWNED_WINDOW_PROPERTY_KEY(ModalWindowController,
                                 kModalWindowControllerKey,
                                 nullptr);

}  // namespace

// static
ModalWindowController* ModalWindowController::Get(aura::Window* container) {
  ModalWindowController* controller =
      container->GetProperty(kModalWindowControllerKey);
  CHECK(controller);
  return controller;
}

ModalWindowController::ModalWindowController(int priority)
    : modal_container_(nullptr),
      dimmer_window_(new aura::Window(nullptr)),
      dimmed_(false) {
  ScreenManager::ContainerParams params("ModalContainer", priority);
  params.can_activate_children = true;
  params.block_events = true;
  modal_container_ = ScreenManager::Get()->CreateContainer(params);
  modal_container_->SetProperty(kModalWindowControllerKey, this);

  dimmer_window_->SetType(ui::wm::WINDOW_TYPE_NORMAL);
  dimmer_window_->Init(aura::WINDOW_LAYER_SOLID_COLOR);
  dimmer_window_->layer()->SetColor(SK_ColorBLACK);
  dimmer_window_->layer()->SetOpacity(0.0f);
  dimmer_window_->Show();

  modal_container_->AddChild(dimmer_window_);
  modal_container_->AddObserver(this);

  UpdateDimmerWindowBounds();
}

ModalWindowController::~ModalWindowController() {
  if (modal_container_)
    modal_container_->RemoveObserver(this);
}

void ModalWindowController::OnWindowAdded(aura::Window* child) {
  DCHECK_NE(child, dimmer_window_);
  if (IsChildWindow(child)) {
    child->AddObserver(this);
    UpdateDimming(nullptr);
  }
}

void ModalWindowController::OnWindowVisibilityChanged(aura::Window* window,
                                                      bool visible) {
  if (IsChildWindow(window))
    UpdateDimming(nullptr);
}

void ModalWindowController::OnWindowBoundsChanged(aura::Window* window,
                                                  const gfx::Rect& old_bounds,
                                                  const gfx::Rect& new_bounds) {
  if (window == modal_container_)
    UpdateDimmerWindowBounds();
}

void ModalWindowController::OnWindowDestroyed(aura::Window* window) {
  UpdateDimming(window);
}

bool ModalWindowController::IsChildWindow(aura::Window* child) const {
  return child->parent() == modal_container_ && child != dimmer_window_;
}

void ModalWindowController::UpdateDimmerWindowBounds() {
  gfx::Rect bounds(modal_container_->bounds().size());
  dimmer_window_->SetBounds(bounds);
}

void ModalWindowController::UpdateDimming(aura::Window* ignore) {
  if (!modal_container_ || !dimmer_window_)
    return;
  bool should_delete = true;
  for (aura::Window* window : modal_container_->children()) {
    if (window == dimmer_window_ || window == ignore)
      continue;
    should_delete = false;
    if (window->TargetVisibility()) {
      SetDimmed(true);
      return;
    }
  }
  SetDimmed(false);

  if (should_delete) {
    // Remove the container from root so that the container becomes
    // invisible, but don't delete it until next event execution
    // because the call stack may still have and use the pointer.
    modal_container_->RemoveObserver(this);

    // Hide the window before removing it, so the focus manager which will run
    // in RemoveChild handler can know that this container is no longer
    // available.
    modal_container_->Hide();

    modal_container_->parent()->RemoveChild(modal_container_);
    base::MessageLoopForUI::current()->DeleteSoon(FROM_HERE, modal_container_);
    modal_container_ = nullptr;
    dimmer_window_ = nullptr;
  }
}

void ModalWindowController::SetDimmed(bool dimmed) {
  const float kDimmedOpacity = 0.4f;

  if (!dimmer_window_ || dimmed_ == dimmed)
    return;
  dimmed_ = dimmed;

  const int kDimmAnimationDurationMs = 500;
  if (dimmed) {
    ui::ScopedLayerAnimationSettings settings(
        dimmer_window_->layer()->GetAnimator());
    settings.SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(kDimmAnimationDurationMs));
    dimmer_window_->layer()->SetOpacity(kDimmedOpacity);
  } else {
    // ScopedHidingAnimationSettings will detach the animating and
    // recreate layers for the container so that animation can continue
    // even if the container is removed immediately.
    wm::ScopedHidingAnimationSettings settings(modal_container_);
    settings.layer_animation_settings()->SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(kDimmAnimationDurationMs));
    modal_container_->layer()->SetOpacity(0.0f);
  }
}

}  // namespace athena
