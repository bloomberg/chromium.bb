// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/web_notification/web_notification_bubble.h"

#include "ash/system/web_notification/web_notification_view.h"
#include "base/bind.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace {
// Delay laying out the WebNotificationBubble until all notifications have been
// added and icons have had a chance to load.
const int kUpdateDelayMs = 50;
const int kNotificationBubbleWidth = 300;
}

namespace message_center {

const SkColor WebNotificationBubble::kBackgroundColor =
    SkColorSetRGB(0xfe, 0xfe, 0xfe);
const SkColor WebNotificationBubble::kHeaderBackgroundColorLight =
    SkColorSetRGB(0xf1, 0xf1, 0xf1);
const SkColor WebNotificationBubble::kHeaderBackgroundColorDark =
    SkColorSetRGB(0xe7, 0xe7, 0xe7);

WebNotificationBubble::WebNotificationBubble(
    WebNotificationList::Delegate* list_delegate)
    : list_delegate_(list_delegate),
      bubble_view_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
}

WebNotificationBubble::~WebNotificationBubble() {
  if (bubble_view_)
    bubble_view_->reset_delegate();
}

void WebNotificationBubble::BubbleViewDestroyed() {
  bubble_view_ = NULL;
  OnBubbleViewDestroyed();
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

TrayBubbleView::InitParams WebNotificationBubble::GetDefaultInitParams(
    TrayBubbleView::AnchorAlignment anchor_alignment) {
  TrayBubbleView::InitParams init_params(TrayBubbleView::ANCHOR_TYPE_TRAY,
                                         anchor_alignment,
                                         kNotificationBubbleWidth);
  init_params.top_color = kBackgroundColor;
  init_params.arrow_color = kHeaderBackgroundColorDark;
  init_params.bubble_width = kWebNotificationWidth;
  return init_params;
}

}  // namespace message_center
