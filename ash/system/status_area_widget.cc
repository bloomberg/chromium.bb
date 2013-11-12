// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/status_area_widget.h"

#include "ash/root_window_controller.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell_window_ids.h"
#include "ash/system/bluetooth/bluetooth_observer.h"
#include "ash/system/logout_button/logout_button_tray.h"
#include "ash/system/status_area_widget_delegate.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/web_notification/web_notification_tray.h"
#include "ash/wm/window_properties.h"
#include "base/i18n/time_formatting.h"
#include "ui/aura/window.h"
#include "ui/gfx/screen.h"

namespace ash {

namespace internal {

const char StatusAreaWidget::kNativeViewName[] = "StatusAreaWidget";

StatusAreaWidget::StatusAreaWidget(aura::Window* status_container)
    : status_area_widget_delegate_(new internal::StatusAreaWidgetDelegate),
      system_tray_(NULL),
      web_notification_tray_(NULL),
      logout_button_tray_(NULL),
      login_status_(user::LOGGED_IN_NONE) {
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.delegate = status_area_widget_delegate_;
  params.parent = status_container;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  Init(params);
  set_focus_on_creation(false);
  SetContentsView(status_area_widget_delegate_);
  GetNativeView()->SetName(kNativeViewName);
}

StatusAreaWidget::~StatusAreaWidget() {
}

void StatusAreaWidget::CreateTrayViews() {
  AddSystemTray();
  AddWebNotificationTray();
  AddLogoutButtonTray();
  SystemTrayDelegate* delegate =
      ash::Shell::GetInstance()->system_tray_delegate();
  DCHECK(delegate);
  // Initialize after all trays have been created.
  if (system_tray_)
    system_tray_->InitializeTrayItems(delegate);
  if (web_notification_tray_)
    web_notification_tray_->Initialize();
  if (logout_button_tray_)
    logout_button_tray_->Initialize();
  UpdateAfterLoginStatusChange(delegate->GetUserLoginStatus());
}

void StatusAreaWidget::Shutdown() {
  // Destroy the trays early, causing them to be removed from the view
  // hierarchy. Do not used scoped pointers since we don't want to destroy them
  // in the destructor if Shutdown() is not called (e.g. in tests).
  delete logout_button_tray_;
  logout_button_tray_ = NULL;
  delete web_notification_tray_;
  web_notification_tray_ = NULL;
  delete system_tray_;
  system_tray_ = NULL;
}

bool StatusAreaWidget::ShouldShowLauncher() const {
  if ((system_tray_ && system_tray_->ShouldShowLauncher()) ||
      (web_notification_tray_ &&
       web_notification_tray_->ShouldBlockLauncherAutoHide()))
    return true;

  if (!RootWindowController::ForLauncher(GetNativeView())->shelf()->IsVisible())
    return false;

  // If the launcher is currently visible, don't hide the launcher if the mouse
  // is in any of the notification bubbles.
  return (system_tray_ && system_tray_->IsMouseInNotificationBubble()) ||
        (web_notification_tray_ &&
         web_notification_tray_->IsMouseInNotificationBubble());
}

bool StatusAreaWidget::IsMessageBubbleShown() const {
  return ((system_tray_ && system_tray_->IsAnyBubbleVisible()) ||
          (web_notification_tray_ &&
           web_notification_tray_->IsMessageCenterBubbleVisible()));
}

void StatusAreaWidget::OnNativeWidgetActivationChanged(bool active) {
  Widget::OnNativeWidgetActivationChanged(active);
  if (active)
    status_area_widget_delegate_->SetPaneFocusAndFocusDefault();
}

void StatusAreaWidget::AddSystemTray() {
  system_tray_ = new SystemTray(this);
  status_area_widget_delegate_->AddTray(system_tray_);
}

void StatusAreaWidget::AddWebNotificationTray() {
  web_notification_tray_ = new WebNotificationTray(this);
  status_area_widget_delegate_->AddTray(web_notification_tray_);
}

void StatusAreaWidget::AddLogoutButtonTray() {
  logout_button_tray_ = new LogoutButtonTray(this);
  status_area_widget_delegate_->AddTray(logout_button_tray_);
}

void StatusAreaWidget::SetShelfAlignment(ShelfAlignment alignment) {
  status_area_widget_delegate_->set_alignment(alignment);
  if (system_tray_)
    system_tray_->SetShelfAlignment(alignment);
  if (web_notification_tray_)
    web_notification_tray_->SetShelfAlignment(alignment);
  if (logout_button_tray_)
    logout_button_tray_->SetShelfAlignment(alignment);
  status_area_widget_delegate_->UpdateLayout();
}

void StatusAreaWidget::SetHideSystemNotifications(bool hide) {
  if (system_tray_)
    system_tray_->SetHideNotifications(hide);
}

void StatusAreaWidget::UpdateAfterLoginStatusChange(
    user::LoginStatus login_status) {
  if (login_status_ == login_status)
    return;
  login_status_ = login_status;
  if (system_tray_)
    system_tray_->UpdateAfterLoginStatusChange(login_status);
  if (web_notification_tray_)
    web_notification_tray_->UpdateAfterLoginStatusChange(login_status);
  if (logout_button_tray_)
    logout_button_tray_->UpdateAfterLoginStatusChange(login_status);
}

}  // namespace internal
}  // namespace ash
