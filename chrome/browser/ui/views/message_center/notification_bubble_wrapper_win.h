// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MESSAGE_CENTER_NOTIFICATION_BUBBLE_WRAPPER_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_MESSAGE_CENTER_NOTIFICATION_BUBBLE_WRAPPER_WIN_H_

#include "chrome/browser/ui/views/message_center/web_notification_tray_win.h"
#include "ui/views/bubble/tray_bubble_view.h"
#include "ui/views/widget/widget.h"

namespace message_center {

namespace internal {

// NotificationBubbleWrapperWin is a class that manages the views associated
// with a MessageBubbleBase object and that notifies the WebNotificationTrayWin
// when the widget closes.  Delegates GetAnchorRect to the
// WebNotificationTrayWin.
class NotificationBubbleWrapperWin
    : public views::WidgetObserver,
      public views::TrayBubbleView::Delegate {
 public:
  enum BubbleType {
    BUBBLE_TYPE_POPUP,
    BUBBLE_TYPE_MESSAGE_CENTER,
  };

  NotificationBubbleWrapperWin(
      WebNotificationTrayWin* tray,
      scoped_ptr<message_center::MessageBubbleBase> bubble,
      BubbleType bubble_type);
  virtual ~NotificationBubbleWrapperWin();

  // Overridden from views::WidgetObserver.
  void OnWidgetDestroying(views::Widget* widget) OVERRIDE;

  // TrayBubbleView::Delegate implementation.
  virtual void BubbleViewDestroyed() OVERRIDE;
  virtual void OnMouseEnteredView() OVERRIDE;
  virtual void OnMouseExitedView() OVERRIDE;
  virtual string16 GetAccessibleNameForBubble() OVERRIDE;
  // GetAnchorRect passes responsibility for BubbleDelegateView::GetAnchorRect
  // to the delegate.
  virtual gfx::Rect GetAnchorRect(views::Widget* anchor_widget,
                                  AnchorType anchor_type,
                                  AnchorAlignment anchor_alignment) OVERRIDE;
  virtual void HideBubble(const views::TrayBubbleView* bubble_view) OVERRIDE;

  // Convenience accessors.
  views::TrayBubbleView* bubble_view() { return bubble_view_; }
  views::Widget* bubble_widget() { return bubble_widget_; }
  message_center::MessageBubbleBase* bubble() { return bubble_.get(); }

 private:
  scoped_ptr<message_center::MessageBubbleBase> bubble_;
  const BubbleType bubble_type_;
  // |bubble_view_| is owned by its Widget.
  views::TrayBubbleView* bubble_view_;
  views::Widget* bubble_widget_;
  WebNotificationTrayWin* tray_;

  DISALLOW_COPY_AND_ASSIGN(NotificationBubbleWrapperWin);
};

}  // namespace internal

}  // namespace message_center

#endif  // CHROME_BROWSER_UI_VIEWS_MESSAGE_CENTER_NOTIFICATION_BUBBLE_WRAPPER_WIN_H_
