// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/status_area_widget.h"

#include "ash/session/session_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/ime_menu/ime_menu_tray.h"
#include "ash/system/overview/overview_button_tray.h"
#include "ash/system/palette/palette_tray.h"
#include "ash/system/session/logout_button_tray.h"
#include "ash/system/status_area_widget_delegate.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/virtual_keyboard/virtual_keyboard_tray.h"
#include "ash/system/web_notification/web_notification_tray.h"
#include "base/i18n/time_formatting.h"
#include "ui/display/display.h"
#include "ui/native_theme/native_theme_dark_aura.h"

namespace ash {

StatusAreaWidget::StatusAreaWidget(aura::Window* status_container, Shelf* shelf)
    : status_area_widget_delegate_(new StatusAreaWidgetDelegate(shelf)),
      shelf_(shelf) {
  DCHECK(status_container);
  DCHECK(shelf);
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.delegate = status_area_widget_delegate_;
  params.name = "StatusAreaWidget";
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.parent = status_container;
  Init(params);
  set_focus_on_creation(false);
  SetContentsView(status_area_widget_delegate_);
}

StatusAreaWidget::~StatusAreaWidget() = default;

void StatusAreaWidget::Initialize() {
  // Create the child views, right to left.
  overview_button_tray_ = new OverviewButtonTray(shelf_);
  status_area_widget_delegate_->AddChildView(overview_button_tray_);

  system_tray_ = new SystemTray(shelf_);
  status_area_widget_delegate_->AddChildView(system_tray_);

  // Must happen after the widget is initialized so the native window exists.
  web_notification_tray_ =
      new WebNotificationTray(shelf_, GetNativeWindow(), system_tray_);
  status_area_widget_delegate_->AddChildView(web_notification_tray_);

  palette_tray_ = new PaletteTray(shelf_);
  status_area_widget_delegate_->AddChildView(palette_tray_);

  virtual_keyboard_tray_ = new VirtualKeyboardTray(shelf_);
  status_area_widget_delegate_->AddChildView(virtual_keyboard_tray_);

  ime_menu_tray_ = new ImeMenuTray(shelf_);
  status_area_widget_delegate_->AddChildView(ime_menu_tray_);

  logout_button_tray_ = new LogoutButtonTray(shelf_);
  status_area_widget_delegate_->AddChildView(logout_button_tray_);

  // The layout depends on the number of children, so build it once after
  // adding all of them.
  status_area_widget_delegate_->UpdateLayout();

  // Initialize after all trays have been created.
  system_tray_->InitializeTrayItems(web_notification_tray_);
  web_notification_tray_->Initialize();
  palette_tray_->Initialize();
  virtual_keyboard_tray_->Initialize();
  ime_menu_tray_->Initialize();
  overview_button_tray_->Initialize();
  UpdateAfterShelfAlignmentChange();
  UpdateAfterLoginStatusChange(
      Shell::Get()->session_controller()->login_status());

  // NOTE: Container may be hidden depending on login/display state.
  Show();
}

void StatusAreaWidget::Shutdown() {
  system_tray_->Shutdown();
  // Destroy the trays early, causing them to be removed from the view
  // hierarchy. Do not used scoped pointers since we don't want to destroy them
  // in the destructor if Shutdown() is not called (e.g. in tests).
  // Failure to remove the tray views causes layout crashes during shutdown,
  // for example http://crbug.com/700122.
  // TODO(jamescook): Find a better way to avoid the layout problems, perhaps
  // by making ShelfWidget call CloseNow() on this widget during shutdown.
  // http://crbug.com/700255
  delete web_notification_tray_;
  web_notification_tray_ = nullptr;
  // Must be destroyed after |web_notification_tray_|.
  delete system_tray_;
  system_tray_ = nullptr;
  delete ime_menu_tray_;
  ime_menu_tray_ = nullptr;
  delete virtual_keyboard_tray_;
  virtual_keyboard_tray_ = nullptr;
  delete palette_tray_;
  palette_tray_ = nullptr;
  delete logout_button_tray_;
  logout_button_tray_ = nullptr;
  delete overview_button_tray_;
  overview_button_tray_ = nullptr;
  // All child tray views have been removed.
  DCHECK_EQ(0, GetContentsView()->child_count());
}

void StatusAreaWidget::UpdateAfterShelfAlignmentChange() {
  system_tray_->UpdateAfterShelfAlignmentChange();
  web_notification_tray_->UpdateAfterShelfAlignmentChange();
  logout_button_tray_->UpdateAfterShelfAlignmentChange();
  virtual_keyboard_tray_->UpdateAfterShelfAlignmentChange();
  ime_menu_tray_->UpdateAfterShelfAlignmentChange();
  palette_tray_->UpdateAfterShelfAlignmentChange();
  overview_button_tray_->UpdateAfterShelfAlignmentChange();
  status_area_widget_delegate_->UpdateLayout();
}

void StatusAreaWidget::UpdateAfterLoginStatusChange(LoginStatus login_status) {
  if (login_status_ == login_status)
    return;
  login_status_ = login_status;

  system_tray_->UpdateAfterLoginStatusChange(login_status);
  logout_button_tray_->UpdateAfterLoginStatusChange();
  overview_button_tray_->UpdateAfterLoginStatusChange(login_status);
}

void StatusAreaWidget::SetSystemTrayVisibility(bool visible) {
  system_tray_->SetVisible(visible);
  // Opacity is set to prevent flakiness in kiosk browser tests. See
  // https://crbug.com/624584.
  SetOpacity(visible ? 1.f : 0.f);
  if (visible) {
    Show();
  } else {
    system_tray_->CloseBubble();
    Hide();
  }
}

TrayBackgroundView* StatusAreaWidget::GetSystemTrayAnchor() const {
  if (overview_button_tray_->visible())
    return overview_button_tray_;
  return system_tray_;
}

bool StatusAreaWidget::ShouldShowShelf() const {
  // The system tray bubble may or may not want to force the shelf to be
  // visible.
  if (system_tray_->IsSystemBubbleVisible())
    return system_tray_->ShouldShowShelf();

  // All other tray bubbles will force the shelf to be visible.
  return views::TrayBubbleView::IsATrayBubbleOpen();
}

bool StatusAreaWidget::IsMessageBubbleShown() const {
  return system_tray_->IsSystemBubbleVisible() ||
         web_notification_tray_->IsMessageCenterVisible();
}

void StatusAreaWidget::SchedulePaint() {
  status_area_widget_delegate_->SchedulePaint();
  web_notification_tray_->SchedulePaint();
  system_tray_->SchedulePaint();
  virtual_keyboard_tray_->SchedulePaint();
  logout_button_tray_->SchedulePaint();
  ime_menu_tray_->SchedulePaint();
  palette_tray_->SchedulePaint();
  overview_button_tray_->SchedulePaint();
}

const ui::NativeTheme* StatusAreaWidget::GetNativeTheme() const {
  return ui::NativeThemeDarkAura::instance();
}

void StatusAreaWidget::OnNativeWidgetActivationChanged(bool active) {
  Widget::OnNativeWidgetActivationChanged(active);
  if (active)
    status_area_widget_delegate_->SetPaneFocusAndFocusDefault();
}

void StatusAreaWidget::UpdateShelfItemBackground(SkColor color) {
  web_notification_tray_->UpdateShelfItemBackground(color);
  system_tray_->UpdateShelfItemBackground(color);
  virtual_keyboard_tray_->UpdateShelfItemBackground(color);
  ime_menu_tray_->UpdateShelfItemBackground(color);
  palette_tray_->UpdateShelfItemBackground(color);
  overview_button_tray_->UpdateShelfItemBackground(color);
}

}  // namespace ash
