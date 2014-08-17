// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/drag_drop/drag_image_view.h"

#include "skia/ext/image_operations.h"
#include "ui/aura/window.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/dip_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/size_conversions.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/shadow_types.h"

namespace ash {
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
  params.opacity = Widget::InitParams::TRANSLUCENT_WINDOW;
  drag_widget->Init(params);
  drag_widget->SetOpacity(0xFF);
  drag_widget->GetNativeWindow()->set_owned_by_parent(false);
  drag_widget->GetNativeWindow()->SetName("DragWidget");
  SetShadowType(drag_widget->GetNativeView(), wm::SHADOW_TYPE_NONE);
  return drag_widget;
}
}

DragImageView::DragImageView(gfx::NativeView context,
                             ui::DragDropTypes::DragEventSource event_source)
    : views::ImageView(),
      drag_event_source_(event_source),
      touch_drag_operation_(ui::DragDropTypes::DRAG_NONE) {
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

gfx::Rect DragImageView::GetBoundsInScreen() const {
  return widget_->GetWindowBoundsInScreen();
}

void DragImageView::SetWidgetVisible(bool visible) {
  if (visible != widget_->IsVisible()) {
    if (visible)
      widget_->Show();
    else
      widget_->Hide();
  }
}

void DragImageView::SetTouchDragOperationHintOff() {
  // Simply set the drag type to non-touch so that no hint is drawn.
  drag_event_source_ = ui::DragDropTypes::DRAG_EVENT_SOURCE_MOUSE;
  SchedulePaint();
}

void DragImageView::SetTouchDragOperation(int operation) {
  if (touch_drag_operation_ == operation)
    return;
  touch_drag_operation_ = operation;
  SchedulePaint();
}

void DragImageView::SetTouchDragOperationHintPosition(
    const gfx::Point& position) {
  if (touch_drag_operation_indicator_position_ == position)
    return;
  touch_drag_operation_indicator_position_ = position;
  SchedulePaint();
}

void DragImageView::SetOpacity(float visibility) {
  DCHECK_GE(visibility, 0.0f);
  DCHECK_LE(visibility, 1.0f);
  widget_->SetOpacity(static_cast<int>(0xff * visibility));
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
    // The drag image already has device scale factor applied. But
    // |widget_size_| is in DIP units.
    gfx::Size scaled_widget_size = gfx::ToRoundedSize(
        gfx::ScaleSize(widget_size_, device_scale));
    gfx::ImageSkiaRep image_rep = GetImage().GetRepresentation(device_scale);
    if (image_rep.is_null())
      return;
    SkBitmap scaled = skia::ImageOperations::Resize(
        image_rep.sk_bitmap(), skia::ImageOperations::RESIZE_LANCZOS3,
        scaled_widget_size.width(), scaled_widget_size.height());
    gfx::ImageSkia image_skia(gfx::ImageSkiaRep(scaled, device_scale));
    canvas->DrawImageInt(image_skia, 0, 0);
  }

  if (drag_event_source_ != ui::DragDropTypes::DRAG_EVENT_SOURCE_TOUCH)
    return;

  // Select appropriate drag hint.
  gfx::Image* drag_hint =
      &ui::ResourceBundle::GetSharedInstance().GetImageNamed(
      IDR_TOUCH_DRAG_TIP_NODROP);
  if (touch_drag_operation_ & ui::DragDropTypes::DRAG_COPY) {
    drag_hint = &ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        IDR_TOUCH_DRAG_TIP_COPY);
  } else if (touch_drag_operation_ & ui::DragDropTypes::DRAG_MOVE) {
    drag_hint = &ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        IDR_TOUCH_DRAG_TIP_MOVE);
  } else if (touch_drag_operation_ & ui::DragDropTypes::DRAG_LINK) {
    drag_hint = &ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        IDR_TOUCH_DRAG_TIP_LINK);
  }
  if (!drag_hint->IsEmpty()) {
    gfx::Size drag_hint_size = drag_hint->Size();

    // Enlarge widget if required to fit the drag hint image.
    if (drag_hint_size.width() > widget_size_.width() ||
        drag_hint_size.height() > widget_size_.height()) {
      gfx::Size new_widget_size = widget_size_;
      new_widget_size.SetToMax(drag_hint_size);
      widget_->SetSize(new_widget_size);
    }

    // Make sure drag hint image is positioned within the widget.
    gfx::Point drag_hint_position = touch_drag_operation_indicator_position_;
    drag_hint_position.Offset(-drag_hint_size.width() / 2, 0);
    gfx::Rect drag_hint_bounds(drag_hint_position, drag_hint_size);
    drag_hint_bounds.AdjustToFit(gfx::Rect(widget_size_));

    // Draw image.
    canvas->DrawImageInt(*(drag_hint->ToImageSkia()),
        drag_hint_bounds.x(), drag_hint_bounds.y());
  }
}

}  // namespace ash
