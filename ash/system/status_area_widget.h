// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_STATUS_AREA_WIDGET_H_
#define ASH_SYSTEM_STATUS_AREA_WIDGET_H_

#include "ash/ash_export.h"
#include "ash/shelf/shelf_types.h"
#include "ash/system/user/login_status.h"
#include "base/macros.h"
#include "ui/views/widget/widget.h"

namespace ash {
class OverviewButtonTray;
class ShelfWidget;
class ShellDelegate;
class StatusAreaWidgetDelegate;
class SystemTray;
class WebNotificationTray;
#if defined(OS_CHROMEOS)
class LogoutButtonTray;
class VirtualKeyboardTray;
#endif

class ASH_EXPORT StatusAreaWidget : public views::Widget {
 public:
  static const char kNativeViewName[];

  StatusAreaWidget(aura::Window* status_container, ShelfWidget* shelf_widget);
  ~StatusAreaWidget() override;

  // Creates the SystemTray, WebNotificationTray and LogoutButtonTray.
  void CreateTrayViews();

  // Destroys the system tray and web notification tray. Called before
  // tearing down the windows to avoid shutdown ordering issues.
  void Shutdown();

  // Update the alignment of the widget and tray views.
  void SetShelfAlignment(ShelfAlignment alignment);

  // Set the visibility of system notifications.
  void SetHideSystemNotifications(bool hide);

  // Called by the client when the login status changes. Caches login_status
  // and calls UpdateAfterLoginStatusChange for the system tray and the web
  // notification tray.
  void UpdateAfterLoginStatusChange(user::LoginStatus login_status);

  StatusAreaWidgetDelegate* status_area_widget_delegate() {
    return status_area_widget_delegate_;
  }
  SystemTray* system_tray() { return system_tray_; }
  WebNotificationTray* web_notification_tray() {
    return web_notification_tray_;
  }
  OverviewButtonTray* overview_button_tray() {
    return overview_button_tray_;
  }
  ShelfWidget* shelf_widget() { return shelf_widget_; }

  user::LoginStatus login_status() const { return login_status_; }

  // Returns true if the shelf should be visible. This is used when the
  // shelf is configured to auto-hide and test if the shelf should force
  // the shelf to remain visible.
  bool ShouldShowShelf() const;

  // True if any message bubble is shown.
  bool IsMessageBubbleShown() const;

  // Notifies child trays, and the |status_area_widget_delegate_| to schedule a
  // paint.
  void SchedulePaint();

  // Overridden from views::Widget:
  void OnNativeWidgetActivationChanged(bool active) override;

 private:
  void AddSystemTray();
  void AddWebNotificationTray();
#if defined(OS_CHROMEOS)
  void AddLogoutButtonTray();
  void AddVirtualKeyboardTray();
#endif
  void AddOverviewButtonTray();

  // Weak pointers to View classes that are parented to StatusAreaWidget:
  StatusAreaWidgetDelegate* status_area_widget_delegate_;
  OverviewButtonTray* overview_button_tray_;
  SystemTray* system_tray_;
  WebNotificationTray* web_notification_tray_;
#if defined(OS_CHROMEOS)
  LogoutButtonTray* logout_button_tray_;
  VirtualKeyboardTray* virtual_keyboard_tray_;
#endif
  user::LoginStatus login_status_;

  ShelfWidget* shelf_widget_;

  DISALLOW_COPY_AND_ASSIGN(StatusAreaWidget);
};

}  // namespace ash

#endif  // ASH_SYSTEM_STATUS_AREA_WIDGET_H_
