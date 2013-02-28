// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MESSAGE_CENTER_WEB_NOTIFICATION_TRAY_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_MESSAGE_CENTER_WEB_NOTIFICATION_TRAY_WIN_H_

#include "chrome/browser/status_icons/status_icon_observer.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/message_center/message_center_tray.h"
#include "ui/message_center/message_center_tray_delegate.h"
#include "ui/views/bubble/tray_bubble_view.h"

class StatusIcon;

namespace message_center {
class MessageCenter;
class MessageCenterBubble;
class MessagePopupBubble;
}

namespace views {
class Widget;
}

namespace message_center {

namespace internal {
class NotificationBubbleWrapperWin;
}

// A MessageCenterTrayDelegate implementation that exposes the MessageCenterTray
// via a system tray icon.  The notification popups will be displayed in the
// corner of the screen and the message center will be displayed by the system
// tray icon on click.
class WebNotificationTrayWin
    : public message_center::MessageCenterTrayDelegate,
      public ui::SimpleMenuModel::Delegate,
      public StatusIconObserver {
 public:
  WebNotificationTrayWin();
  virtual ~WebNotificationTrayWin();

  message_center::MessageCenter* message_center();

  // MessageCenterTrayDelegate implementation.
  virtual bool ShowPopups() OVERRIDE;
  virtual void HidePopups() OVERRIDE;
  virtual bool ShowMessageCenter() OVERRIDE;
  virtual void HideMessageCenter() OVERRIDE;
  virtual void UpdateMessageCenter() OVERRIDE;
  virtual void UpdatePopups() OVERRIDE;
  virtual void OnMessageCenterTrayChanged() OVERRIDE;

  // These are forwarded to WebNotificationTrayWin by
  // NotificationBubbleWrapperWin classes since they don't have enough
  // context to provide the required data for TrayBubbleView::Delegate.
  gfx::Rect GetMessageCenterAnchor();
  gfx::Rect GetPopupAnchor();
  gfx::NativeView GetBubbleWindowContainer();
  views::TrayBubbleView::AnchorAlignment GetAnchorAlignment();

  // StatusIconObserver implementation.
  virtual void OnStatusIconClicked() OVERRIDE;

  // Changes the icon and hovertext based on number of unread notifications.
  void UpdateStatusIcon();
  void HideBubbleWithView(const views::TrayBubbleView* bubble_view);

  // SimpleMenuModel::Delegate implementation.
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE;
  virtual void ExecuteCommand(int command_id) OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(WebNotificationTrayWinTest, WebNotifications);
  FRIEND_TEST_ALL_PREFIXES(WebNotificationTrayWinTest,
                           WebNotificationPopupBubble);
  FRIEND_TEST_ALL_PREFIXES(WebNotificationTrayWinTest,
                           ManyMessageCenterNotifications);
  FRIEND_TEST_ALL_PREFIXES(WebNotificationTrayWinTest, ManyPopupNotifications);

  void AddQuietModeMenu(StatusIcon* status_icon);
  message_center::MessagePopupBubble* GetPopupBubbleForTest();
  message_center::MessageCenterBubble* GetMessageCenterBubbleForTest();

  scoped_ptr<internal::NotificationBubbleWrapperWin> popup_bubble_;
  scoped_ptr<internal::NotificationBubbleWrapperWin> message_center_bubble_;

  StatusIcon* status_icon_;
  bool message_center_visible_;
  scoped_ptr<MessageCenterTray> message_center_tray_;
  gfx::Point mouse_click_point_;

  DISALLOW_COPY_AND_ASSIGN(WebNotificationTrayWin);
};

}  // namespace message_center

#endif  // CHROME_BROWSER_UI_VIEWS_MESSAGE_CENTER_WEB_NOTIFICATION_TRAY_WIN_H_
