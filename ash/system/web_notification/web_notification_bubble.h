// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_WEB_NOTIFICATION_WEB_NOTIFICATION_BUBBLE_H_
#define ASH_SYSTEM_WEB_NOTIFICATION_WEB_NOTIFICATION_BUBBLE_H_

#include "ash/ash_export.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "ash/system/web_notification/web_notification_list.h"
#include "base/memory/scoped_ptr.h"

namespace message_center {

class WebNotificationContentsView;
class WebNotificationView;

class ASH_EXPORT WebNotificationBubble {
 public:
  explicit WebNotificationBubble(WebNotificationList::Delegate* list_delegate);

  virtual ~WebNotificationBubble();

  // Gets called when when the bubble view associated with this bubble is
  // destroyed. Clears |bubble_view_| and calls OnBubbleViewDestroyed.
  void BubbleViewDestroyed();

  // Gets the init params for the implementation.
  virtual TrayBubbleView::InitParams GetInitParams(
      TrayBubbleView::AnchorAlignment anchor_alignment) = 0;

  // Called after the bubble view has been constructed. Creates and initializes
  // the bubble contents.
  virtual void InitializeContents(TrayBubbleView* bubble_view) = 0;

  // Called from BubbleViewDestroyed for implementation specific details.
  virtual void OnBubbleViewDestroyed() = 0;

  // Updates the bubble; implementation dependent.
  virtual void UpdateBubbleView() = 0;

  // Called when the mouse enters/exists the view.
  virtual void OnMouseEnteredView() = 0;
  virtual void OnMouseExitedView() = 0;

  // Schedules bubble for layout after all notifications have been
  // added and icons have had a chance to load.
  void ScheduleUpdate();

  bool IsVisible() const;

  TrayBubbleView* bubble_view() const { return bubble_view_; }

  static const SkColor kBackgroundColor;
  static const SkColor kHeaderBackgroundColorLight;
  static const SkColor kHeaderBackgroundColorDark;

 protected:
  TrayBubbleView::InitParams GetDefaultInitParams(
      TrayBubbleView::AnchorAlignment anchor_alignment);

  WebNotificationList::Delegate* list_delegate_;
  TrayBubbleView* bubble_view_;
  base::WeakPtrFactory<WebNotificationBubble> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebNotificationBubble);
};

}  // namespace message_center

#endif // ASH_SYSTEM_WEB_NOTIFICATION_WEB_NOTIFICATION_BUBBLE_H_
