// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/web_notification/web_notification_bubble.h"

#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/web_notification/web_notification_tray.h"
#include "ash/system/web_notification/web_notification_view.h"
#include "base/bind.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace ash {

namespace message_center {

// Delay laying out the WebNotificationBubble until all notifications have been
// added and icons have had a chance to load.
const int kUpdateDelayMs = 50;

WebNotificationBubble::WebNotificationBubble(WebNotificationTray* tray)
    : tray_(tray),
      bubble_view_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
}

void WebNotificationBubble::Initialize(views::View* contents_view) {
  DCHECK(bubble_view_);
  bubble_view_->AddChildView(contents_view);

  bubble_wrapper_.reset(new internal::TrayBubbleWrapper(tray_, bubble_view_));
  UpdateBubbleView();
}

WebNotificationBubble::~WebNotificationBubble() {
  if (bubble_view_)
    bubble_view_->reset_delegate();
}

void WebNotificationBubble::ScheduleUpdate() {
  weak_ptr_factory_.InvalidateWeakPtrs();  // Cancel any pending update.
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&WebNotificationBubble::UpdateBubbleView,
                 weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(kUpdateDelayMs));
}

bool WebNotificationBubble::IsVisible() const {
  return bubble_view() && bubble_view()->GetWidget()->IsVisible();
}

void WebNotificationBubble::BubbleViewDestroyed() {
  bubble_view_ = NULL;
}

void WebNotificationBubble::OnMouseEnteredView() {
}

void WebNotificationBubble::OnMouseExitedView() {
}

string16 WebNotificationBubble::GetAccessibleName() {
  return tray_->GetAccessibleName();
}

gfx::Rect WebNotificationBubble::GetAnchorRect(
    views::Widget* anchor_widget,
    TrayBubbleView::AnchorType anchor_type,
    TrayBubbleView::AnchorAlignment anchor_alignment) {
  return tray_->GetAnchorRect(anchor_widget, anchor_type, anchor_alignment);
}

TrayBubbleView::InitParams WebNotificationBubble::GetInitParams() {
  TrayBubbleView::AnchorAlignment anchor_alignment =
      tray_->GetAnchorAlignment();
  TrayBubbleView::InitParams init_params(TrayBubbleView::ANCHOR_TYPE_TRAY,
                                         anchor_alignment,
                                         kTrayPopupWidth);
  init_params.top_color = kBackgroundColor;
  init_params.arrow_color = kHeaderBackgroundColorDark;
  init_params.bubble_width = kWebNotificationWidth;
  if (anchor_alignment == TrayBubbleView::ANCHOR_ALIGNMENT_BOTTOM) {
    views::View* anchor = tray_->tray_container();
    gfx::Point bounds(anchor->width() / 2, 0);

    views::View::ConvertPointToWidget(anchor, &bounds);
    init_params.arrow_offset = bounds.x();
  }
  return init_params;
}

}  // namespace message_center

}  // namespace ash
