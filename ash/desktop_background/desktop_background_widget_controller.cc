// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/desktop_background/desktop_background_widget_controller.h"

#include "ash/ash_export.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window_property.h"
#include "ui/views/widget/widget.h"

// Exported for tests.
DECLARE_EXPORTED_WINDOW_PROPERTY_TYPE(
    ASH_EXPORT, ash::internal::DesktopBackgroundWidgetController*);
DECLARE_EXPORTED_WINDOW_PROPERTY_TYPE(
    ASH_EXPORT, ash::internal::AnimatingDesktopController*);

namespace ash {
namespace internal {

DEFINE_OWNED_WINDOW_PROPERTY_KEY(DesktopBackgroundWidgetController,
                                 kDesktopController, NULL);
DEFINE_OWNED_WINDOW_PROPERTY_KEY(AnimatingDesktopController,
                                 kAnimatingDesktopController, NULL);

DesktopBackgroundWidgetController::DesktopBackgroundWidgetController(
    views::Widget* widget) : widget_(widget) {
  DCHECK(widget_);
  widget_->AddObserver(this);
}

DesktopBackgroundWidgetController::DesktopBackgroundWidgetController(
    ui::Layer* layer) : widget_(NULL) {
  layer_.reset(layer);
}

DesktopBackgroundWidgetController::~DesktopBackgroundWidgetController() {
  if (widget_) {
    widget_->RemoveObserver(this);
    widget_->CloseNow();
    widget_ = NULL;
  } else if (layer_)
    layer_.reset(NULL);
}

void DesktopBackgroundWidgetController::OnWidgetDestroying(
    views::Widget* widget) {
  widget_->RemoveObserver(this);
  widget_ = NULL;
}

void DesktopBackgroundWidgetController::SetBounds(gfx::Rect bounds) {
  if (widget_)
    widget_->SetBounds(bounds);
  else if (layer_)
    layer_->SetBounds(bounds);
}

bool DesktopBackgroundWidgetController::Reparent(aura::RootWindow* root_window,
                                                 int src_container,
                                                 int dest_container) {
  if (widget_) {
    views::Widget::ReparentNativeView(widget_->GetNativeView(),
        root_window->GetChildById(dest_container));
    return true;
  }
  if (layer_) {
    ui::Layer* layer = layer_.get();
    root_window->GetChildById(src_container)->layer()->Remove(layer);
    root_window->GetChildById(dest_container)->layer()->Add(layer);
    return true;
  }
  // Nothing to reparent.
  return false;
}

AnimatingDesktopController::AnimatingDesktopController(
    DesktopBackgroundWidgetController* component) {
  controller_.reset(component);
}

AnimatingDesktopController::~AnimatingDesktopController() {
}

void AnimatingDesktopController::StopAnimating() {
  if (controller_) {
    ui::Layer* layer = controller_->layer() ? controller_->layer() :
        controller_->widget()->GetNativeView()->layer();
    layer->GetAnimator()->StopAnimating();
  }
}

DesktopBackgroundWidgetController* AnimatingDesktopController::GetController(
    bool pass_ownership) {
  if (pass_ownership)
    return controller_.release();
  return controller_.get();
}

}  // namespace internal
}  // namespace ash
