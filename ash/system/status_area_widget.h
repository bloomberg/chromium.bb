// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_STATUS_AREA_WIDGET_H_
#define ASH_SYSTEM_STATUS_AREA_WIDGET_H_
#pragma once

#include "ash/ash_export.h"
#include "ash/launcher/background_animator.h"
#include "ash/system/user/login_status.h"
#include "ash/wm/shelf_types.h"
#include "ui/views/widget/widget.h"

namespace ash {

class ShellDelegate;
class SystemTray;
class SystemTrayDelegate;
class WebNotificationTray;

namespace internal {

class StatusAreaWidgetDelegate;

class ASH_EXPORT StatusAreaWidget : public views::Widget {
 public:
  enum UserAction {
    NON_USER_ACTION,
    USER_ACTION
  };

  StatusAreaWidget();
  virtual ~StatusAreaWidget();

  // Creates the SystemTray and the WebNotificationTray.
  void CreateTrayViews(ShellDelegate* shell_delegate);

  // Destroys the system tray and web notification tray. Called before
  // tearing down the windows to avoid shutdown ordering issues.
  void Shutdown();

  // Update the alignment of the widget and tray views.
  void SetShelfAlignment(ShelfAlignment alignment);

  // Update whether to paint a background for each tray view.
  void SetPaintsBackground(
      bool value,
      internal::BackgroundAnimator::ChangeType change_type);

  // Always used to show/hide the web notification tray. These handle any logic
  // with hiding/supressing notifications from the system tray.
  void ShowWebNotificationBubble(UserAction user_action);
  void HideWebNotificationBubble();

  // Called by the client when the login status changes. Caches login_status
  // and calls UpdateAfterLoginStatusChange for the system tray and the web
  // notification tray.
  void UpdateAfterLoginStatusChange(user::LoginStatus login_status);

  SystemTray* system_tray() { return system_tray_; }
  SystemTrayDelegate* system_tray_delegate() {
    return system_tray_delegate_.get();
  }
  WebNotificationTray* web_notification_tray() {
    return web_notification_tray_;
  }

  user::LoginStatus login_status() const { return login_status_; }

 private:
  void AddSystemTray(SystemTray* system_tray, ShellDelegate* shell_delegate);
  void AddWebNotificationTray(WebNotificationTray* web_notification_tray);

  scoped_ptr<SystemTrayDelegate> system_tray_delegate_;
  // Weak pointers to View classes that are parented to StatusAreaWidget:
  internal::StatusAreaWidgetDelegate* widget_delegate_;
  SystemTray* system_tray_;
  WebNotificationTray* web_notification_tray_;
  user::LoginStatus login_status_;

  DISALLOW_COPY_AND_ASSIGN(StatusAreaWidget);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_STATUS_AREA_WIDGET_H_
