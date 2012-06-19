// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/drag_drop/drag_image_view.h"

#include "ash/wm/shadow_types.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace internal {

namespace {
using views::Widget;

Widget* CreateDragWidget() {
  Widget* drag_widget = new Widget;
  Widget::InitParams params;
  params.type = Widget::InitParams::TYPE_TOOLTIP;
  params.keep_on_top = true;
  params.accept_events = false;
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.transparent = true;
  drag_widget->Init(params);
  drag_widget->SetOpacity(0xFF);
  SetShadowType(drag_widget->GetNativeView(), SHADOW_TYPE_NONE);
  return drag_widget;
}
}

DragImageView::DragImageView() : views::ImageView() {
  widget_.reset(CreateDragWidget());
  widget_->SetContentsView(this);
  widget_->SetAlwaysOnTop(true);

  // We are owned by the DragDropController.
  set_owned_by_client();

  // The drag image we receive is already drawn scaled to device scale factor.
  // Hence we do not need our layer to apply device scale again.
  aura::Window* window = widget_->GetNativeView();
  if (window && window->layer())
    window->layer()->set_scale_content(false);
}

DragImageView::~DragImageView() {
  widget_->Hide();
}

void DragImageView::SetScreenBounds(const gfx::Rect& bounds) {
  widget_->SetBounds(bounds);
}

void DragImageView::SetScreenPosition(const gfx::Point& position) {
  widget_->SetBounds(gfx::Rect(position, GetPreferredSize()));
}

void DragImageView::SetWidgetVisible(bool visible) {
  if (visible != widget_->IsVisible()) {
    if (visible)
      widget_->Show();
    else
      widget_->Hide();
  }
}

}  // namespace internal
}  // namespace ash
