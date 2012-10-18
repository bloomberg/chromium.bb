// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/web_notification/popup_bubble.h"

#include "ash/system/tray/tray_bubble_view.h"
#include "ash/system/web_notification/web_notification_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace message_center {

const int kAutocloseDelaySeconds = 5;

// Popup notifications contents.
class PopupBubbleContentsView : public views::View {
 public:
  explicit PopupBubbleContentsView(
      WebNotificationList::Delegate* list_delegate)
      : list_delegate_(list_delegate) {
    SetLayoutManager(
        new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 1));

    content_ = new views::View;
    content_->SetLayoutManager(
        new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 1));
    AddChildView(content_);

    content_->SetPaintToLayer(true);
    content_->SetFillsBoundsOpaquely(false);
    content_->layer()->SetMasksToBounds(true);
  }

  void Update(const WebNotificationList::Notifications& popup_notifications) {
    content_->RemoveAllChildViews(true);
    for (WebNotificationList::Notifications::const_iterator iter =
             popup_notifications.begin();
         iter != popup_notifications.end(); ++iter) {
      WebNotificationView* view = new WebNotificationView(
          list_delegate_, *iter, 0);
      content_->AddChildView(view);
    }
    content_->SizeToPreferredSize();
    content_->InvalidateLayout();
    Layout();
    if (GetWidget())
      GetWidget()->GetRootView()->SchedulePaint();
  }

  size_t NumMessageViews() const {
    return content_->child_count();
  }

 private:
  WebNotificationList::Delegate* list_delegate_;
  views::View* content_;

  DISALLOW_COPY_AND_ASSIGN(PopupBubbleContentsView);
};

// PopupBubble
PopupBubble::PopupBubble(WebNotificationList::Delegate* delegate) :
    WebNotificationBubble(delegate),
    contents_view_(NULL),
    num_popups_(0) {
}

PopupBubble::~PopupBubble() {
}

TrayBubbleView::InitParams PopupBubble::GetInitParams(
    TrayBubbleView::AnchorAlignment anchor_alignment) {
  TrayBubbleView::InitParams init_params =
      GetDefaultInitParams(anchor_alignment);
  init_params.top_color = kBackgroundColor;
  init_params.arrow_color = kBackgroundColor;
  init_params.close_on_deactivate = false;
  return init_params;
}

void PopupBubble::InitializeContents(TrayBubbleView* bubble_view) {
  bubble_view_ = bubble_view;
  contents_view_ = new PopupBubbleContentsView(list_delegate_);
  bubble_view_->AddChildView(contents_view_);
  UpdateBubbleView();
}

void PopupBubble::OnBubbleViewDestroyed() {
  contents_view_ = NULL;
}

void PopupBubble::UpdateBubbleView() {
  WebNotificationList::Notifications popup_notifications;
  list_delegate_->GetNotificationList()->GetPopupNotifications(
      &popup_notifications);
  if (popup_notifications.size() == 0) {
    if (bubble_view())
      bubble_view()->delegate()->HideBubble(bubble_view());  // deletes |this|
    return;
  }
  // Only reset the timer when the number of visible notifications changes.
  if (num_popups_ != popup_notifications.size()) {
    num_popups_ = popup_notifications.size();
    StartAutoCloseTimer();
  }
  contents_view_->Update(popup_notifications);
  bubble_view_->Show();
  bubble_view_->UpdateBubble();
}

void PopupBubble::OnMouseEnteredView() {
  StopAutoCloseTimer();
}

void PopupBubble::OnMouseExitedView() {
  StartAutoCloseTimer();
}

void PopupBubble::StartAutoCloseTimer() {
  autoclose_.Start(FROM_HERE,
                   base::TimeDelta::FromSeconds(
                       kAutocloseDelaySeconds),
                   this, &PopupBubble::OnAutoClose);
}

void PopupBubble::StopAutoCloseTimer() {
  autoclose_.Stop();
}

void PopupBubble::OnAutoClose() {
  list_delegate_->GetNotificationList()->MarkPopupsAsShown();
  num_popups_ = 0;
  UpdateBubbleView();
}

size_t PopupBubble::NumMessageViewsForTest() const {
  return contents_view_->NumMessageViews();
}

}  // namespace message_center
