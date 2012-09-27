// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NOTIFICATION_MESSAGE_CENTER_BUBBLE_H_
#define ASH_SYSTEM_NOTIFICATION_MESSAGE_CENTER_BUBBLE_H_

#include "ash/system/web_notification/web_notification_bubble.h"

namespace ash {

class WebNotificationTray;

namespace message_center {

class MessageCenterContentsView;

// Bubble for message center.
class MessageCenterBubble : public WebNotificationBubble {
 public:
  explicit MessageCenterBubble(WebNotificationTray* tray);

  virtual ~MessageCenterBubble();

  size_t NumMessageViewsForTest() const;

  // Overridden from TrayBubbleView::Host.
  virtual void BubbleViewDestroyed() OVERRIDE;

  virtual void OnClickedOutsideView() OVERRIDE;

 private:
  // Overridden from WebNotificationBubble.
  virtual void UpdateBubbleView() OVERRIDE;

  MessageCenterContentsView* contents_view_;

  DISALLOW_COPY_AND_ASSIGN(MessageCenterBubble);
};

}  // namespace message_center

}  // namespace ash

#endif // ASH_SYSTEM_NOTIFICATION_MESSAGE_CENTER_BUBBLE_H_
