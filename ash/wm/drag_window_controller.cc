// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/drag_window_controller.h"

#include "ash/shell_window_ids.h"
#include "ash/wm/window_util.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/shadow_types.h"
#include "ui/wm/core/window_util.h"

namespace ash {

DragWindowController::DragWindowController(aura::Window* window)
    : window_(window),
      drag_widget_(NULL) {
}

DragWindowController::~DragWindowController() {
  Hide();
}

void DragWindowController::SetDestinationDisplay(
    const gfx::Display& dst_display) {
  dst_display_ = dst_display;
}

void DragWindowController::Show() {
  if (!drag_widget_)
    CreateDragWidget(window_->GetBoundsInScreen());
  drag_widget_->Show();
}

void DragWindowController::SetBounds(const gfx::Rect& bounds) {
  DCHECK(drag_widget_);
  bounds_ = bounds;
  SetBoundsInternal(bounds);
}

void DragWindowController::Hide() {
  if (drag_widget_) {
    drag_widget_->Close();
    drag_widget_ = NULL;
  }
  layer_owner_.reset();
}

void DragWindowController::SetOpacity(float opacity) {
  DCHECK(drag_widget_);
  ui::Layer* layer = drag_widget_->GetNativeWindow()->layer();
  ui::ScopedLayerAnimationSettings scoped_setter(layer->GetAnimator());
  layer->SetOpacity(opacity);
}

void DragWindowController::CreateDragWidget(const gfx::Rect& bounds) {
  DCHECK(!drag_widget_);
  drag_widget_ = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.parent = window_->parent();
  params.keep_on_top = true;
  drag_widget_->set_focus_on_creation(false);
  drag_widget_->Init(params);
  drag_widget_->SetVisibilityChangedAnimationsEnabled(false);
  drag_widget_->GetNativeWindow()->SetName("DragWindow");
  drag_widget_->GetNativeWindow()->set_id(kShellWindowId_PhantomWindow);
  // Show shadow for the dragging window.
  SetShadowType(drag_widget_->GetNativeWindow(),
                ::wm::SHADOW_TYPE_RECTANGULAR);
  SetBoundsInternal(bounds);
  drag_widget_->StackAbove(window_);

  RecreateWindowLayers();
  aura::Window* window = drag_widget_->GetNativeWindow();
  layer_owner_->root()->SetVisible(true);
  window->layer()->Add(layer_owner_->root());
  window->layer()->StackAtTop(layer_owner_->root());

  // Show the widget after all the setups.
  drag_widget_->Show();

  // Fade the window in.
  ui::Layer* widget_layer = drag_widget_->GetNativeWindow()->layer();
  widget_layer->SetOpacity(0);
  ui::ScopedLayerAnimationSettings scoped_setter(widget_layer->GetAnimator());
  widget_layer->SetOpacity(1);
}

void DragWindowController::SetBoundsInternal(const gfx::Rect& bounds) {
  aura::Window* window = drag_widget_->GetNativeWindow();
  aura::client::ScreenPositionClient* screen_position_client =
      aura::client::GetScreenPositionClient(window->GetRootWindow());
  if (screen_position_client && dst_display_.is_valid())
    screen_position_client->SetBounds(window, bounds, dst_display_);
  else
    drag_widget_->SetBounds(bounds);
}

void DragWindowController::RecreateWindowLayers() {
  DCHECK(!layer_owner_.get());
  layer_owner_ = ::wm::RecreateLayers(window_);
  layer_owner_->root()->set_delegate(window_->layer()->delegate());
  // Place the layer at (0, 0) of the DragWindowController's window.
  gfx::Rect layer_bounds = layer_owner_->root()->bounds();
  layer_bounds.set_origin(gfx::Point(0, 0));
  layer_owner_->root()->SetBounds(layer_bounds);
  layer_owner_->root()->SetVisible(false);
  // Detach it from the current container.
  layer_owner_->root()->parent()->Remove(layer_owner_->root());
}

}  // namespace ash
