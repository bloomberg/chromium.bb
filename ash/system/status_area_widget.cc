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
#include "ash/system/overview/overview_button_tray.h"
#include "ash/system/status_area_widget_delegate.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/web_notification/web_notification_tray.h"
#include "ash/wm/window_properties.h"
#include "base/i18n/time_formatting.h"
#include "ui/aura/window.h"

#if defined(OS_CHROMEOS)
#include "ash/system/chromeos/session/logout_button_tray.h"
#include "ash/system/chromeos/virtual_keyboard/virtual_keyboard_tray.h"
#endif

namespace ash {

const char StatusAreaWidget::kNativeViewName[] = "StatusAreaWidget";

StatusAreaWidget::StatusAreaWidget(aura::Window* status_container,
                                   ShelfWidget* shelf_widget)
    : status_area_widget_delegate_(new StatusAreaWidgetDelegate),
      overview_button_tray_(NULL),
      system_tray_(NULL),
      web_notification_tray_(NULL),
#if defined(OS_CHROMEOS)
      logout_button_tray_(NULL),
      virtual_keyboard_tray_(NULL),
#endif
      login_status_(user::LOGGED_IN_NONE),
      shelf_widget_(shelf_widget) {
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
  AddOverviewButtonTray();
  AddSystemTray();
  AddWebNotificationTray();
#if defined(OS_CHROMEOS)
  AddLogoutButtonTray();
  AddVirtualKeyboardTray();
#endif

  SystemTrayDelegate* delegate =
      ash::Shell::GetInstance()->system_tray_delegate();
  DCHECK(delegate);
  // Initialize after all trays have been created.
  system_tray_->InitializeTrayItems(delegate);
  web_notification_tray_->Initialize();
#if defined(OS_CHROMEOS)
  logout_button_tray_->Initialize();
  virtual_keyboard_tray_->Initialize();
#endif
  overview_button_tray_->Initialize();
  UpdateAfterLoginStatusChange(delegate->GetUserLoginStatus());
}

void StatusAreaWidget::Shutdown() {
  // Destroy the trays early, causing them to be removed from the view
  // hierarchy. Do not used scoped pointers since we don't want to destroy them
  // in the destructor if Shutdown() is not called (e.g. in tests).
  delete web_notification_tray_;
  web_notification_tray_ = NULL;
  delete system_tray_;
  system_tray_ = NULL;
#if defined(OS_CHROMEOS)
  delete virtual_keyboard_tray_;
  virtual_keyboard_tray_ = NULL;
  delete logout_button_tray_;
  logout_button_tray_ = NULL;
#endif
  delete overview_button_tray_;
  overview_button_tray_ = NULL;
}

bool StatusAreaWidget::ShouldShowShelf() const {
  if ((system_tray_ && system_tray_->ShouldShowShelf()) ||
      (web_notification_tray_ &&
       web_notification_tray_->ShouldBlockShelfAutoHide()))
    return true;

  if (!shelf_widget_->shelf()->IsVisible())
    return false;

  // If the shelf is currently visible, don't hide the shelf if the mouse
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

void StatusAreaWidget::SchedulePaint() {
  status_area_widget_delegate_->SchedulePaint();
  web_notification_tray_->SchedulePaint();
  system_tray_->SchedulePaint();
#if defined(OS_CHROMEOS)
  virtual_keyboard_tray_->SchedulePaint();
  logout_button_tray_->SchedulePaint();
#endif
  overview_button_tray_->SchedulePaint();
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

#if defined(OS_CHROMEOS)
void StatusAreaWidget::AddLogoutButtonTray() {
  logout_button_tray_ = new LogoutButtonTray(this);
  status_area_widget_delegate_->AddTray(logout_button_tray_);
}

void StatusAreaWidget::AddVirtualKeyboardTray() {
  virtual_keyboard_tray_ = new VirtualKeyboardTray(this);
  status_area_widget_delegate_->AddTray(virtual_keyboard_tray_);
}
#endif

void StatusAreaWidget::AddOverviewButtonTray() {
  overview_button_tray_ = new OverviewButtonTray(this);
  status_area_widget_delegate_->AddTray(overview_button_tray_);
}

void StatusAreaWidget::SetShelfAlignment(wm::ShelfAlignment alignment) {
  status_area_widget_delegate_->set_alignment(alignment);
  if (system_tray_)
    system_tray_->SetShelfAlignment(alignment);
  if (web_notification_tray_)
    web_notification_tray_->SetShelfAlignment(alignment);
#if defined(OS_CHROMEOS)
  if (logout_button_tray_)
    logout_button_tray_->SetShelfAlignment(alignment);
  if (virtual_keyboard_tray_)
    virtual_keyboard_tray_->SetShelfAlignment(alignment);
#endif
  if (overview_button_tray_)
    overview_button_tray_->SetShelfAlignment(alignment);
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
#if defined(OS_CHROMEOS)
  if (logout_button_tray_)
    logout_button_tray_->UpdateAfterLoginStatusChange(login_status);
#endif
  if (overview_button_tray_)
    overview_button_tray_->UpdateAfterLoginStatusChange(login_status);
}

}  // namespace ash
