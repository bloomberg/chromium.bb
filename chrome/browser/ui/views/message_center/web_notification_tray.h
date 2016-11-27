// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MESSAGE_CENTER_WEB_NOTIFICATION_TRAY_H_
#define CHROME_BROWSER_UI_VIEWS_MESSAGE_CENTER_WEB_NOTIFICATION_TRAY_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "ui/message_center/message_center_tray_delegate.h"
#include "ui/views/widget/widget_observer.h"

namespace message_center {
class DesktopPopupAlignmentDelegate;
class MessageCenter;
class MessageCenterTray;
class MessagePopupCollection;

// A MessageCenterTrayDelegate implementation that exposes the MessageCenterTray
// via a system tray icon.  The notification popups will be displayed in the
// corner of the screen and the message center will be displayed by the system
// tray icon on click.
class WebNotificationTray : public message_center::MessageCenterTrayDelegate {
 public:
  WebNotificationTray();
  ~WebNotificationTray() override;

  message_center::MessageCenter* message_center();

  // MessageCenterTrayDelegate implementation.
  bool ShowPopups() override;
  void HidePopups() override;
  bool ShowMessageCenter() override;
  void HideMessageCenter() override;
  void OnMessageCenterTrayChanged() override;
  bool ShowNotifierSettings() override;
  bool IsContextMenuEnabled() const override;

  MessageCenterTray* GetMessageCenterTray() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(WebNotificationTrayTest, WebNotifications);
  FRIEND_TEST_ALL_PREFIXES(WebNotificationTrayTest, WebNotificationPopupBubble);
  FRIEND_TEST_ALL_PREFIXES(WebNotificationTrayTest, ManyPopupNotifications);
  FRIEND_TEST_ALL_PREFIXES(WebNotificationTrayTest, ManuallyCloseMessageCenter);

  std::unique_ptr<MessagePopupCollection> popup_collection_;
  std::unique_ptr<DesktopPopupAlignmentDelegate> alignment_delegate_;

  std::unique_ptr<MessageCenterTray> message_center_tray_;
  DISALLOW_COPY_AND_ASSIGN(WebNotificationTray);
};

}  // namespace message_center

#endif  // CHROME_BROWSER_UI_VIEWS_MESSAGE_CENTER_WEB_NOTIFICATION_TRAY_H_
