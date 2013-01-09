// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/drag_drop/drag_image_view.h"

#include "skia/ext/image_operations.h"
#include "ui/aura/window.h"
#include "ui/compositor/dip_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/size_conversions.h"
#include "ui/views/corewm/shadow_types.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace internal {

namespace {
using views::Widget;

Widget* CreateDragWidget(gfx::NativeView context) {
  Widget* drag_widget = new Widget;
  Widget::InitParams params;
  params.type = Widget::InitParams::TYPE_TOOLTIP;
  params.keep_on_top = true;
  params.context = context;
  params.accept_events = false;
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.transparent = true;
  drag_widget->Init(params);
  drag_widget->SetOpacity(0xFF);
  drag_widget->GetNativeWindow()->set_owned_by_parent(false);
  SetShadowType(drag_widget->GetNativeView(), views::corewm::SHADOW_TYPE_NONE);
  return drag_widget;
}
}

DragImageView::DragImageView(gfx::NativeView context) : views::ImageView() {
  widget_.reset(CreateDragWidget(context));
  widget_->SetContentsView(this);
  widget_->SetAlwaysOnTop(true);

  // We are owned by the DragDropController.
  set_owned_by_client();
}

DragImageView::~DragImageView() {
  widget_->Hide();
}

void DragImageView::SetBoundsInScreen(const gfx::Rect& bounds) {
  widget_->SetBounds(bounds);
  widget_size_ = bounds.size();
}

void DragImageView::SetScreenPosition(const gfx::Point& position) {
  widget_->SetBounds(gfx::Rect(position, widget_size_));
}

void DragImageView::SetWidgetVisible(bool visible) {
  if (visible != widget_->IsVisible()) {
    if (visible)
      widget_->Show();
    else
      widget_->Hide();
  }
}

void DragImageView::OnPaint(gfx::Canvas* canvas) {
  if (GetImage().isNull())
    return;

  // |widget_size_| is in DIP. ImageSkia::size() also returns the size in DIP.
  if (GetImage().size() == widget_size_) {
    canvas->DrawImageInt(GetImage(), 0, 0);
  } else {
    float device_scale = 1;
    if (widget_->GetNativeView() && widget_->GetNativeView()->layer()) {
      device_scale = ui::GetDeviceScaleFactor(
          widget_->GetNativeView()->layer());
    }
    ui::ScaleFactor device_scale_factor =
        ui::GetScaleFactorFromScale(device_scale);

    // The drag image already has device scale factor applied. But
    // |widget_size_| is in DIP units.
    gfx::Size scaled_widget_size = gfx::ToRoundedSize(
        gfx::ScaleSize(widget_size_, device_scale));
    gfx::ImageSkiaRep image_rep = GetImage().GetRepresentation(
        device_scale_factor);
    if (image_rep.is_null())
      return;
    SkBitmap scaled = skia::ImageOperations::Resize(
        image_rep.sk_bitmap(), skia::ImageOperations::RESIZE_LANCZOS3,
        scaled_widget_size.width(), scaled_widget_size.height());
    gfx::ImageSkia image_skia(gfx::ImageSkiaRep(scaled, device_scale_factor));
    canvas->DrawImageInt(image_skia, 0, 0);
  }
}

}  // namespace internal
}  // namespace ash
