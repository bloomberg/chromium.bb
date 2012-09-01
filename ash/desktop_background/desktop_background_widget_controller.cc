// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/desktop_background/desktop_background_widget_controller.h"

#include "ui/aura/root_window.h"
#include "ui/views/widget/widget.h"

DECLARE_WINDOW_PROPERTY_TYPE(ash::internal::DesktopBackgroundWidgetController*);
DECLARE_WINDOW_PROPERTY_TYPE(ash::internal::ComponentWrapper*);

namespace ash {
namespace internal {

DEFINE_OWNED_WINDOW_PROPERTY_KEY(DesktopBackgroundWidgetController,
                                 kWindowDesktopComponent, NULL);
DEFINE_OWNED_WINDOW_PROPERTY_KEY(ComponentWrapper, kComponentWrapper, NULL);

DesktopBackgroundWidgetController::DesktopBackgroundWidgetController(
    views::Widget* widget) : widget_(widget) {
}

DesktopBackgroundWidgetController::DesktopBackgroundWidgetController(
    ui::Layer* layer) : widget_(NULL) {
  layer_.reset(layer);
}

DesktopBackgroundWidgetController::~DesktopBackgroundWidgetController() {
  if (widget_) {
    widget_->CloseNow();
    widget_ = NULL;
  } else if (layer_.get())
    layer_.reset(NULL);
}

void DesktopBackgroundWidgetController::CleanupWidget() {
  widget_ = NULL;
}

void DesktopBackgroundWidgetController::SetBounds(gfx::Rect bounds) {
  if (widget_)
    widget_->SetBounds(bounds);
  else if (layer_.get())
    layer_->SetBounds(bounds);
}

void DesktopBackgroundWidgetController::Reparent(aura::RootWindow* root_window,
                                                 int src_container,
                                                 int dest_container) {
  if (widget_) {
    views::Widget::ReparentNativeView(widget_->GetNativeView(),
        root_window->GetChildById(dest_container));
  } else if (layer_.get()) {
    ui::Layer* layer = layer_.get();
    root_window->GetChildById(src_container)->layer()->Remove(layer);
    root_window->GetChildById(dest_container)->layer()->Add(layer);
  }
}

ComponentWrapper::ComponentWrapper(
    DesktopBackgroundWidgetController* component) {
  component_.reset(component);
}

ComponentWrapper::~ComponentWrapper() {
}

DesktopBackgroundWidgetController* ComponentWrapper::GetComponent(
    bool pass_ownership) {
  if (pass_ownership)
    return component_.release();
  return component_.get();
}

}  // namespace internal
}  // namespace ash
