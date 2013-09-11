// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MESSAGE_CENTER_WEB_NOTIFICATION_TRAY_H_
#define CHROME_BROWSER_UI_VIEWS_MESSAGE_CENTER_WEB_NOTIFICATION_TRAY_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/status_icons/status_icon_menu_model.h"
#include "chrome/browser/status_icons/status_icon_observer.h"
#include "chrome/browser/ui/views/message_center/message_center_widget_delegate.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/gfx/rect.h"
#include "ui/message_center/message_center_tray.h"
#include "ui/message_center/message_center_tray_delegate.h"
#include "ui/views/widget/widget_observer.h"

class StatusIcon;

namespace message_center {
class MessageCenter;
class MessagePopupCollection;
}

namespace views {
class Widget;
}

namespace message_center {

struct PositionInfo;

class MessageCenterWidgetDelegate;

// A MessageCenterTrayDelegate implementation that exposes the MessageCenterTray
// via a system tray icon.  The notification popups will be displayed in the
// corner of the screen and the message center will be displayed by the system
// tray icon on click.
class WebNotificationTray : public message_center::MessageCenterTrayDelegate,
                            public StatusIconObserver,
                            public base::SupportsWeakPtr<WebNotificationTray>,
                            public StatusIconMenuModel::Delegate {
 public:
  WebNotificationTray();
  virtual ~WebNotificationTray();

  message_center::MessageCenter* message_center();

  // MessageCenterTrayDelegate implementation.
  virtual bool ShowPopups() OVERRIDE;
  virtual void HidePopups() OVERRIDE;
  virtual bool ShowMessageCenter() OVERRIDE;
  virtual void HideMessageCenter() OVERRIDE;
  virtual void OnMessageCenterTrayChanged() OVERRIDE;
  virtual bool ShowNotifierSettings() OVERRIDE;

  // StatusIconObserver implementation.
  virtual void OnStatusIconClicked() OVERRIDE;
#if defined(OS_WIN)
  virtual void OnBalloonClicked() OVERRIDE;

  // This shows a platform-specific balloon informing the user of the existence
  // of the message center in the status tray area.
  void DisplayFirstRunBalloon();
#endif

  // StatusIconMenuModel::Delegate implementation.
  virtual void ExecuteCommand(int command_id, int event_flags) OVERRIDE;

  // Changes the icon and hovertext based on number of unread notifications.
  void UpdateStatusIcon();
  void SendHideMessageCenter();
  void MarkMessageCenterHidden();

  // Gets the point where the status icon was clicked.
  gfx::Point mouse_click_point() { return mouse_click_point_; }
  virtual MessageCenterTray* GetMessageCenterTray() OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(WebNotificationTrayTest, WebNotifications);
  FRIEND_TEST_ALL_PREFIXES(WebNotificationTrayTest, WebNotificationPopupBubble);
  FRIEND_TEST_ALL_PREFIXES(WebNotificationTrayTest,
                           ManyMessageCenterNotifications);
  FRIEND_TEST_ALL_PREFIXES(WebNotificationTrayTest, ManyPopupNotifications);

  PositionInfo GetPositionInfo();

  void CreateStatusIcon(const gfx::ImageSkia& image, const string16& tool_tip);
  void DestroyStatusIcon();
  void AddQuietModeMenu(StatusIcon* status_icon);
  MessageCenterWidgetDelegate* GetMessageCenterWidgetDelegateForTest();

  MessageCenterWidgetDelegate* message_center_delegate_;
  scoped_ptr<message_center::MessagePopupCollection> popup_collection_;

  StatusIcon* status_icon_;
  StatusIconMenuModel* status_icon_menu_;
  bool message_center_visible_;
  scoped_ptr<MessageCenterTray> message_center_tray_;
  gfx::Point mouse_click_point_;

  bool should_update_tray_content_;
  bool last_quiet_mode_state_;

  DISALLOW_COPY_AND_ASSIGN(WebNotificationTray);
};

}  // namespace message_center

#endif  // CHROME_BROWSER_UI_VIEWS_MESSAGE_CENTER_WEB_NOTIFICATION_TRAY_H_
