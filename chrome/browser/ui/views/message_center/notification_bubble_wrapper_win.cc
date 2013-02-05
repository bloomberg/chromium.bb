// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/message_center/notification_bubble_wrapper_win.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/views/message_center/web_notification_tray_win.h"
#include "ui/gfx/size.h"
#include "ui/message_center/message_bubble_base.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/bubble/tray_bubble_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace {

const char kAccessibleNameForBubble[] = "Windows Notification Center";

}

namespace message_center {

namespace internal {

NotificationBubbleWrapperWin::NotificationBubbleWrapperWin(
    WebNotificationTrayWin* tray,
    scoped_ptr<message_center::MessageBubbleBase> bubble,
    AnchorType anchor_type)
    : bubble_(bubble.Pass()),
      bubble_view_(NULL),
      bubble_widget_(NULL),
      tray_(tray) {
  // Windows-specific initialization.
  views::TrayBubbleView::AnchorAlignment anchor_alignment =
      tray->GetAnchorAlignment();
  views::TrayBubbleView::InitParams init_params =
      bubble_->GetInitParams(anchor_alignment);
  init_params.anchor_type = anchor_type;
  init_params.close_on_deactivate = false;
  init_params.arrow_alignment =
      views::BubbleBorder::ALIGN_ARROW_TO_MID_ANCHOR;
  // TODO(dewittj): Show big shadow without blocking clicks.
  init_params.shadow = views::BubbleBorder::NO_SHADOW;

  bubble_view_ = views::TrayBubbleView::Create(
      tray->GetBubbleWindowContainer(), NULL, this, &init_params);

  bubble_widget_ = views::BubbleDelegateView::CreateBubble(bubble_view_);
  bubble_widget_->AddObserver(this);
  bubble_widget_->StackAtTop();
  bubble_widget_->SetAlwaysOnTop(true);
  bubble_widget_->Activate();
  bubble_view_->InitializeAndShowBubble();

  bubble_view_->set_close_on_deactivate(true);
  bubble_->InitializeContents(bubble_view_);
}

NotificationBubbleWrapperWin::~NotificationBubbleWrapperWin() {
  bubble_.reset();
  if (bubble_widget_) {
    bubble_widget_->RemoveObserver(this);
    bubble_widget_->Close();
  }
}

void NotificationBubbleWrapperWin::OnWidgetDestroying(views::Widget* widget) {
  DCHECK_EQ(widget, bubble_widget_);
  bubble_widget_->RemoveObserver(this);
  bubble_widget_ = NULL;
  tray_->HideBubbleWithView(bubble_view_);
}

void NotificationBubbleWrapperWin::BubbleViewDestroyed() {
  bubble_->BubbleViewDestroyed();
}

void NotificationBubbleWrapperWin::OnMouseEnteredView() {
  bubble_->OnMouseEnteredView();
}

void NotificationBubbleWrapperWin::OnMouseExitedView() {
  bubble_->OnMouseExitedView();
}

string16 NotificationBubbleWrapperWin::GetAccessibleNameForBubble() {
  // TODO(dewittj): Get a string resource.
  return ASCIIToUTF16(kAccessibleNameForBubble);
}

gfx::Rect NotificationBubbleWrapperWin::GetAnchorRect(
    views::Widget* anchor_widget,
    AnchorType anchor_type,
    AnchorAlignment anchor_alignment) {
  gfx::Size size = bubble_view_->GetPreferredSize();
  return tray_->GetAnchorRect(size,
                              anchor_type,
                              anchor_alignment);
}

void NotificationBubbleWrapperWin::HideBubble(
    const views::TrayBubbleView* bubble_view) {
  tray_->HideBubbleWithView(bubble_view);
}

}  // namespace internal

}  // namespace message_center
