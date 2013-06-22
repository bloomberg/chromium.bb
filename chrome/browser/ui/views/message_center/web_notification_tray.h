// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MESSAGE_CENTER_WEB_NOTIFICATION_TRAY_H_
#define CHROME_BROWSER_UI_VIEWS_MESSAGE_CENTER_WEB_NOTIFICATION_TRAY_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/status_icons/status_icon_observer.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/message_center/message_center_tray.h"
#include "ui/message_center/message_center_tray_delegate.h"
#include "ui/views/bubble/tray_bubble_view.h"

class StatusIcon;

namespace message_center {
class MessageCenter;
class MessageCenterBubble;
class MessagePopupCollection;
}

namespace views {
class Widget;
}

namespace message_center {

namespace internal {
class NotificationBubbleWrapper;
}

// A MessageCenterTrayDelegate implementation that exposes the MessageCenterTray
// via a system tray icon.  The notification popups will be displayed in the
// corner of the screen and the message center will be displayed by the system
// tray icon on click.
class WebNotificationTray : public message_center::MessageCenterTrayDelegate,
                            public StatusIconObserver,
                            public base::SupportsWeakPtr<WebNotificationTray> {
 public:
  WebNotificationTray();
  virtual ~WebNotificationTray();

  message_center::MessageCenter* message_center();

  // MessageCenterTrayDelegate implementation.
  virtual bool ShowPopups() OVERRIDE;
  virtual void HidePopups() OVERRIDE;
  virtual bool ShowMessageCenter() OVERRIDE;
  virtual void HideMessageCenter() OVERRIDE;
  virtual void UpdatePopups() OVERRIDE;
  virtual void OnMessageCenterTrayChanged() OVERRIDE;
  virtual bool ShowNotifierSettings() OVERRIDE;

  // These are forwarded to WebNotificationTray by
  // NotificationBubbleWrapper classes since they don't have enough
  // context to provide the required data for TrayBubbleView::Delegate.
  gfx::Rect GetMessageCenterAnchor();
  gfx::Rect GetPopupAnchor();
  gfx::NativeView GetBubbleWindowContainer();
  views::TrayBubbleView::AnchorAlignment GetAnchorAlignment();

  // StatusIconObserver implementation.
  virtual void OnStatusIconClicked() OVERRIDE;
#if defined(OS_WIN)
  virtual void OnBalloonClicked() OVERRIDE;

  // This shows a platform-specific balloon informing the user of the existence
  // of the message center in the status tray area.
  void DisplayFirstRunBalloon();
#endif

  // Changes the icon and hovertext based on number of unread notifications.
  void UpdateStatusIcon();
  void HideBubbleWithView(const views::TrayBubbleView* bubble_view);

 private:
  FRIEND_TEST_ALL_PREFIXES(WebNotificationTrayTest, WebNotifications);
  FRIEND_TEST_ALL_PREFIXES(WebNotificationTrayTest, WebNotificationPopupBubble);
  FRIEND_TEST_ALL_PREFIXES(WebNotificationTrayTest,
                           ManyMessageCenterNotifications);
  FRIEND_TEST_ALL_PREFIXES(WebNotificationTrayTest, ManyPopupNotifications);

  // The actual process to show the message center. Set |show_settings| to true
  // if the message center should be initialized with the settings visible.
  // Returns true if the center is successfully created.
  bool ShowMessageCenterInternal(bool show_settings);
  StatusIcon* GetStatusIcon();
  void DestroyStatusIcon();
  void AddQuietModeMenu(StatusIcon* status_icon);
  message_center::MessageCenterBubble* GetMessageCenterBubbleForTest();

  scoped_ptr<internal::NotificationBubbleWrapper> message_center_bubble_;
  scoped_ptr<message_center::MessagePopupCollection> popup_collection_;

  StatusIcon* status_icon_;
  bool message_center_visible_;
  scoped_ptr<MessageCenterTray> message_center_tray_;
  gfx::Point mouse_click_point_;

  bool should_update_tray_content_;

  DISALLOW_COPY_AND_ASSIGN(WebNotificationTray);
};

}  // namespace message_center

#endif  // CHROME_BROWSER_UI_VIEWS_MESSAGE_CENTER_WEB_NOTIFICATION_TRAY_H_
