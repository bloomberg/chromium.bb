// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/web_notification/popup_bubble.h"

#include "ash/system/tray/tray_bubble_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/web_notification/web_notification_contents_view.h"
#include "ash/system/web_notification/web_notification_list.h"
#include "ash/system/web_notification/web_notification_tray.h"
#include "ash/system/web_notification/web_notification_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

using internal::TrayBubbleView;

namespace message_center {

const int kAutocloseDelaySeconds = 5;

// Popup notifications contents.
class PopupBubbleContentsView : public WebNotificationContentsView {
 public:
  explicit PopupBubbleContentsView(WebNotificationTray* tray)
      : WebNotificationContentsView(tray) {
    SetLayoutManager(
        new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 1));
    set_background(views::Background::CreateSolidBackground(kBackgroundColor));

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
      WebNotificationView* view = new WebNotificationView(tray_, *iter);
      content_->AddChildView(view);
    }
    content_->SizeToPreferredSize();
    Layout();
    if (GetWidget())
      GetWidget()->GetRootView()->SchedulePaint();
  }

  size_t NumMessageViewsForTest() const {
    return content_->child_count();
  }

 private:
  views::View* content_;

  DISALLOW_COPY_AND_ASSIGN(PopupBubbleContentsView);
};

// PopupBubble
PopupBubble::PopupBubble(WebNotificationTray* tray) :
    WebNotificationBubble(tray),
    contents_view_(NULL),
    num_popups_(0) {
  TrayBubbleView::InitParams init_params = GetInitParams();
  init_params.arrow_color = kBackgroundColor;
  init_params.close_on_deactivate = false;
  views::View* anchor = tray_->tray_container();
  bubble_view_ = TrayBubbleView::Create(anchor, this, init_params);
  contents_view_ = new PopupBubbleContentsView(tray);

  Initialize(contents_view_);
}

PopupBubble::~PopupBubble() {}

size_t PopupBubble::NumMessageViewsForTest() const {
  return contents_view_->NumMessageViewsForTest();
}

void PopupBubble::BubbleViewDestroyed() {
  contents_view_ = NULL;
  WebNotificationBubble::BubbleViewDestroyed();
}

void PopupBubble::OnMouseEnteredView() {
  StopAutoCloseTimer();
  WebNotificationBubble::OnMouseEnteredView();
}

void PopupBubble::OnMouseExitedView() {
  StartAutoCloseTimer();
  WebNotificationBubble::OnMouseExitedView();
}

void PopupBubble::UpdateBubbleView() {
  WebNotificationList::Notifications popup_notifications;
  tray_->notification_list()->GetPopupNotifications(&popup_notifications);
  if (popup_notifications.size() == 0) {
    tray_->HideBubble(this);  // deletes |this|!
    return;
  }
  // Only update the popup tray if the number of visible popup notifications
  // has changed.
  if (popup_notifications.size() != num_popups_) {
    num_popups_ = popup_notifications.size();
    contents_view_->Update(popup_notifications);
    bubble_view_->Show();
    bubble_view_->UpdateBubble();
    StartAutoCloseTimer();
  }
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
  tray_->notification_list()->MarkPopupsAsShown();
  num_popups_ = 0;
  UpdateBubbleView();
}

}  // namespace message_center

}  // namespace ash
