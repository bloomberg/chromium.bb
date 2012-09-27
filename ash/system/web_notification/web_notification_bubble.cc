// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/web_notification/web_notification_bubble.h"

#include "ash/system/web_notification/web_notification_contents_view.h"
#include "ash/system/web_notification/web_notification_tray.h"
#include "ash/system/web_notification/web_notification_view.h"
#include "base/bind.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace ash {

using internal::TrayBubbleView;

namespace message_center {

// Delay laying out the WebNotificationBubble until all notifications have been
// added and icons have had a chance to load.
const int kUpdateDelayMs = 50;

WebNotificationBubble::WebNotificationBubble(WebNotificationTray* tray)
    : tray_(tray),
      bubble_view_(NULL),
      bubble_widget_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
}

void WebNotificationBubble::Initialize(
    WebNotificationContentsView* contents_view) {
  DCHECK(bubble_view_);

  bubble_view_->AddChildView(contents_view);

  bubble_widget_ = views::BubbleDelegateView::CreateBubble(bubble_view_);
  bubble_widget_->AddObserver(this);

  InitializeAndShowBubble(bubble_widget_, bubble_view_, tray_);
  UpdateBubbleView();
}

WebNotificationBubble::~WebNotificationBubble() {
  if (bubble_view_)
    bubble_view_->reset_host();
  if (bubble_widget_) {
    bubble_widget_->RemoveObserver(this);
    bubble_widget_->Close();
  }
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
  return bubble_widget_ && bubble_widget_->IsVisible();
}

void WebNotificationBubble::BubbleViewDestroyed() {
  bubble_view_ = NULL;
}

void WebNotificationBubble::OnMouseEnteredView() {
}

void WebNotificationBubble::OnMouseExitedView() {
}

void WebNotificationBubble::OnClickedOutsideView() {
  // May delete |this|.
  tray_->HideMessageCenterBubble();
}

string16 WebNotificationBubble::GetAccessibleName() {
  return tray_->GetAccessibleName();
}

// Overridden from views::WidgetObserver:
void WebNotificationBubble::OnWidgetClosing(views::Widget* widget) {
  CHECK_EQ(bubble_widget_, widget);
  bubble_widget_ = NULL;
  tray_->HideBubble(this);  // Will destroy |this|.
}

TrayBubbleView::InitParams WebNotificationBubble::GetInitParams() {
  TrayBubbleView::InitParams init_params(TrayBubbleView::ANCHOR_TYPE_TRAY,
                                         tray_->shelf_alignment());
  init_params.bubble_width = kWebNotificationWidth;
  if (tray_->shelf_alignment() == SHELF_ALIGNMENT_BOTTOM) {
    views::View* anchor = tray_->tray_container();
    gfx::Point bounds(anchor->width() / 2, 0);

    views::View::ConvertPointToWidget(anchor, &bounds);
    init_params.arrow_offset = bounds.x();
  }
  return init_params;
}

}  // namespace message_center

}  // namespace ash
