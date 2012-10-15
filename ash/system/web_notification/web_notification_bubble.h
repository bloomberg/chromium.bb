// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NOTIFICATION_WEB_NOTIFICATION_BUBBLE_H_
#define ASH_SYSTEM_NOTIFICATION_WEB_NOTIFICATION_BUBBLE_H_

#include "ash/system/tray/tray_bubble_view.h"
#include "base/memory/scoped_ptr.h"

namespace ash {

class WebNotificationTray;

namespace internal {
class TrayBubbleWrapper;
}

namespace message_center {

class WebNotificationContentsView;
class WebNotificationView;

class WebNotificationBubble : public TrayBubbleView::Delegate {
 public:
  explicit WebNotificationBubble(WebNotificationTray* tray);

  virtual ~WebNotificationBubble();

  void Initialize(views::View* contents_view);

  // Updates the bubble; implementation dependent.
  virtual void UpdateBubbleView() = 0;

  // Schedules bubble for layout after all notifications have been
  // added and icons have had a chance to load.
  void ScheduleUpdate();

  bool IsVisible() const;

  TrayBubbleView* bubble_view() const { return bubble_view_; }

  // Overridden from TrayBubbleView::Delegate.
  virtual void BubbleViewDestroyed() OVERRIDE;
  virtual void OnMouseEnteredView() OVERRIDE;
  virtual void OnMouseExitedView() OVERRIDE;
  virtual string16 GetAccessibleName() OVERRIDE;
  virtual gfx::Rect GetAnchorRect(views::Widget* anchor_widget,
                                  AnchorType anchor_type,
                                  AnchorAlignment anchor_alignment) OVERRIDE;

 protected:
  TrayBubbleView::InitParams GetInitParams();

  WebNotificationTray* tray_;
  TrayBubbleView* bubble_view_;
  scoped_ptr<internal::TrayBubbleWrapper> bubble_wrapper_;
  base::WeakPtrFactory<WebNotificationBubble> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebNotificationBubble);
};

}  // namespace message_center

}  // namespace ash

#endif // ASH_SYSTEM_NOTIFICATION_WEB_NOTIFICATION_BUBBLE_H_
