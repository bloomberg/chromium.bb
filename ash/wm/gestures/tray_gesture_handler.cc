// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/gestures/tray_gesture_handler.h"

#include "ash/shell.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_bubble.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/gfx/transform.h"
#include "ui/views/widget/widget.h"

const int kMinBubbleHeight = 13;

namespace ash {

TrayGestureHandler::TrayGestureHandler()
    : widget_(NULL),
      gesture_drag_amount_(0) {
  // TODO(oshima): Support multiple display case.
  SystemTray* tray = Shell::GetInstance()->GetPrimarySystemTray();
  tray->ShowDefaultView(BUBBLE_CREATE_NEW);
  SystemTrayBubble* bubble = tray->GetSystemBubble();
  if (!bubble)
    return;
  bubble->bubble_view()->set_gesture_dragging(true);
  widget_ = bubble->bubble_view()->GetWidget();
  widget_->AddObserver(this);

  gfx::Rect bounds = widget_->GetWindowBoundsInScreen();
  int height_change = bounds.height() - kMinBubbleHeight;
  bounds.set_height(kMinBubbleHeight);
  bounds.set_y(bounds.y() + height_change);
  widget_->SetBounds(bounds);
}

TrayGestureHandler::~TrayGestureHandler() {
  if (widget_)
    widget_->RemoveObserver(this);
}

bool TrayGestureHandler::UpdateGestureDrag(const ui::GestureEvent& event) {
  CHECK_EQ(ui::ET_GESTURE_SCROLL_UPDATE, event.type());
  if (!widget_)
    return false;

  gesture_drag_amount_ += event.details().scroll_y();
  if (gesture_drag_amount_ > 0 && gesture_drag_amount_ < kMinBubbleHeight) {
    widget_->Close();
    return false;
  }

  gfx::Rect bounds = widget_->GetWindowBoundsInScreen();
  int new_height = std::min(
      kMinBubbleHeight + std::max(0, static_cast<int>(-gesture_drag_amount_)),
      widget_->GetContentsView()->GetPreferredSize().height());
  int height_change = bounds.height() - new_height;
  bounds.set_height(new_height);
  bounds.set_y(bounds.y() + height_change);
  widget_->SetBounds(bounds);
  return true;
}

void TrayGestureHandler::CompleteGestureDrag(const ui::GestureEvent& event) {
  if (!widget_)
    return;

  widget_->RemoveObserver(this);

  // Close the widget if it hasn't been dragged enough.
  bool should_close = false;
  int height = widget_->GetWindowBoundsInScreen().height();
  int preferred_height =
      widget_->GetContentsView()->GetPreferredSize().height();
  if (event.type() == ui::ET_GESTURE_SCROLL_END) {
    const float kMinThresholdGestureDrag = 0.4f;
    if (height < preferred_height * kMinThresholdGestureDrag)
      should_close = true;
  } else if (event.type() == ui::ET_SCROLL_FLING_START) {
    const float kMinThresholdGestureDragExposeFling = 0.25f;
    const float kMinThresholdGestureFling = 1000.f;
    if (height < preferred_height * kMinThresholdGestureDragExposeFling &&
        event.details().velocity_y() > -kMinThresholdGestureFling)
      should_close = true;
  } else {
    NOTREACHED();
  }

  if (should_close) {
    widget_->Close();
  } else {
    SystemTrayBubble* bubble =
        Shell::GetInstance()->GetPrimarySystemTray()->GetSystemBubble();
    if (bubble)
      bubble->bubble_view()->set_gesture_dragging(false);
  }
}

void TrayGestureHandler::OnWidgetDestroying(views::Widget* widget) {
  CHECK_EQ(widget_, widget);
  widget_ = NULL;
}

}  // namespace ash
