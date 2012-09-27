// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NOTIFICATION_POPUP_BUBBLE_H_
#define ASH_SYSTEM_NOTIFICATION_POPUP_BUBBLE_H_

#include "ash/system/web_notification/web_notification_bubble.h"
#include "base/timer.h"

namespace ash {

class WebNotificationTray;

namespace message_center {

class PopupBubbleContentsView;

// Bubble for popup notifications.
class PopupBubble : public WebNotificationBubble {
 public:
  explicit PopupBubble(WebNotificationTray* tray);

  virtual ~PopupBubble();

  size_t NumMessageViewsForTest() const;

  // Overridden from TrayBubbleView::Host.
  virtual void BubbleViewDestroyed() OVERRIDE;

  virtual void OnMouseEnteredView() OVERRIDE;

  virtual void OnMouseExitedView() OVERRIDE;

 private:
  // Overridden from WebNotificationBubble.
  virtual void UpdateBubbleView() OVERRIDE;

  void StartAutoCloseTimer();

  void StopAutoCloseTimer();

  void OnAutoClose();

  base::OneShotTimer<PopupBubble> autoclose_;
  PopupBubbleContentsView* contents_view_;
  size_t num_popups_;

  DISALLOW_COPY_AND_ASSIGN(PopupBubble);
};

}  // namespace message_center

}  // namespace ash

#endif // ASH_SYSTEM_NOTIFICATION_POPUP_BUBBLE_H_
