// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_STATUS_AREA_WIDGET_H_
#define ASH_SYSTEM_STATUS_AREA_WIDGET_H_

#include "ash/ash_export.h"
#include "ash/shelf/shelf_types.h"
#include "ash/system/user/login_status.h"
#include "ui/views/widget/widget.h"

namespace ash {

class ShellDelegate;
class SystemTray;
class WebNotificationTray;

namespace internal {

class StatusAreaWidgetDelegate;

class ASH_EXPORT StatusAreaWidget : public views::Widget {
 public:
  explicit StatusAreaWidget(aura::Window* status_container);
  virtual ~StatusAreaWidget();

  // Creates the SystemTray and the WebNotificationTray.
  void CreateTrayViews();

  // Destroys the system tray and web notification tray. Called before
  // tearing down the windows to avoid shutdown ordering issues.
  void Shutdown();

  // Update the alignment of the widget and tray views.
  void SetShelfAlignment(ShelfAlignment alignment);

  // Set the visibility state of web notifications.
  void SetHideWebNotifications(bool hide);

  // Set the visibility of system notifications.
  void SetHideSystemNotifications(bool hide);

  // Returns true if it is OK to show a non system notification.
  bool ShouldShowWebNotifications();

  // Called by the client when the login status changes. Caches login_status
  // and calls UpdateAfterLoginStatusChange for the system tray and the web
  // notification tray.
  void UpdateAfterLoginStatusChange(user::LoginStatus login_status);

  internal::StatusAreaWidgetDelegate* status_area_widget_delegate() {
    return status_area_widget_delegate_;
  }
  SystemTray* system_tray() { return system_tray_; }
  WebNotificationTray* web_notification_tray() {
    return web_notification_tray_;
  }

  user::LoginStatus login_status() const { return login_status_; }

  // Returns true if the launcher should be visible. This is used when the
  // launcher is configured to auto-hide and test if the shelf should force
  // the launcher to remain visible.
  bool ShouldShowLauncher() const;

  // True if any message bubble is shown.
  bool IsMessageBubbleShown() const;

 private:
  void AddSystemTray();
  void AddWebNotificationTray();

  // Weak pointers to View classes that are parented to StatusAreaWidget:
  internal::StatusAreaWidgetDelegate* status_area_widget_delegate_;
  SystemTray* system_tray_;
  WebNotificationTray* web_notification_tray_;
  user::LoginStatus login_status_;

  DISALLOW_COPY_AND_ASSIGN(StatusAreaWidget);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_STATUS_AREA_WIDGET_H_
