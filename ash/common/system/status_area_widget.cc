// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/status_area_widget.h"

#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/shelf/wm_shelf.h"
#include "ash/common/system/chromeos/ime_menu/ime_menu_tray.h"
#include "ash/common/system/chromeos/palette/palette_tray.h"
#include "ash/common/system/chromeos/palette/palette_utils.h"
#include "ash/common/system/chromeos/session/logout_button_tray.h"
#include "ash/common/system/chromeos/virtual_keyboard/virtual_keyboard_tray.h"
#include "ash/common/system/overview/overview_button_tray.h"
#include "ash/common/system/status_area_widget_delegate.h"
#include "ash/common/system/tray/system_tray.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/system/web_notification/web_notification_tray.h"
#include "ash/common/wm_window.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "base/i18n/time_formatting.h"
#include "ui/display/display.h"
#include "ui/native_theme/native_theme_dark_aura.h"

namespace ash {

StatusAreaWidget::StatusAreaWidget(WmWindow* status_container,
                                   WmShelf* wm_shelf)
    : status_area_widget_delegate_(new StatusAreaWidgetDelegate),
      overview_button_tray_(nullptr),
      system_tray_(nullptr),
      web_notification_tray_(nullptr),
      logout_button_tray_(nullptr),
      palette_tray_(nullptr),
      virtual_keyboard_tray_(nullptr),
      ime_menu_tray_(nullptr),
      login_status_(LoginStatus::NOT_LOGGED_IN),
      wm_shelf_(wm_shelf) {
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.delegate = status_area_widget_delegate_;
  params.name = "StatusAreaWidget";
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  status_container->GetRootWindowController()
      ->ConfigureWidgetInitParamsForContainer(
          this, status_container->GetShellWindowId(), &params);
  Init(params);
  set_focus_on_creation(false);
  SetContentsView(status_area_widget_delegate_);
}

StatusAreaWidget::~StatusAreaWidget() {}

void StatusAreaWidget::CreateTrayViews() {
  AddOverviewButtonTray();
  AddSystemTray();
  AddWebNotificationTray();
  AddPaletteTray();
  AddVirtualKeyboardTray();
  AddImeMenuTray();
  AddLogoutButtonTray();

  SystemTrayDelegate* delegate = Shell::Get()->system_tray_delegate();
  DCHECK(delegate);
  // Initialize after all trays have been created.
  system_tray_->InitializeTrayItems(delegate, web_notification_tray_);
  web_notification_tray_->Initialize();
  logout_button_tray_->Initialize();
  if (palette_tray_)
    palette_tray_->Initialize();
  virtual_keyboard_tray_->Initialize();
  ime_menu_tray_->Initialize();
  overview_button_tray_->Initialize();
  SetShelfAlignment(system_tray_->shelf_alignment());
  UpdateAfterLoginStatusChange(delegate->GetUserLoginStatus());
}

void StatusAreaWidget::Shutdown() {
  system_tray_->Shutdown();
  // Destroy the trays early, causing them to be removed from the view
  // hierarchy. Do not used scoped pointers since we don't want to destroy them
  // in the destructor if Shutdown() is not called (e.g. in tests).
  // Failure to remove the tray views causes layout crashes during shutdown,
  // for example http://crbug.com/700122.
  // TODO(jamescook): Find a better way to avoid the layout problems, fix the
  // tests and switch to std::unique_ptr. http://crbug.com/700255
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

void StatusAreaWidget::SetShelfAlignment(ShelfAlignment alignment) {
  status_area_widget_delegate_->set_alignment(alignment);
  if (system_tray_)
    system_tray_->SetShelfAlignment(alignment);
  if (web_notification_tray_)
    web_notification_tray_->SetShelfAlignment(alignment);
  if (logout_button_tray_)
    logout_button_tray_->SetShelfAlignment(alignment);
  if (virtual_keyboard_tray_)
    virtual_keyboard_tray_->SetShelfAlignment(alignment);
  if (ime_menu_tray_)
    ime_menu_tray_->SetShelfAlignment(alignment);
  if (palette_tray_)
    palette_tray_->SetShelfAlignment(alignment);
  if (overview_button_tray_)
    overview_button_tray_->SetShelfAlignment(alignment);
  status_area_widget_delegate_->UpdateLayout();
}

void StatusAreaWidget::UpdateAfterLoginStatusChange(LoginStatus login_status) {
  if (login_status_ == login_status)
    return;
  login_status_ = login_status;
  if (system_tray_)
    system_tray_->UpdateAfterLoginStatusChange(login_status);
  if (web_notification_tray_)
    web_notification_tray_->UpdateAfterLoginStatusChange(login_status);
  if (logout_button_tray_)
    logout_button_tray_->UpdateAfterLoginStatusChange(login_status);
  if (overview_button_tray_)
    overview_button_tray_->UpdateAfterLoginStatusChange(login_status);
}

bool StatusAreaWidget::ShouldShowShelf() const {
  if ((system_tray_ && system_tray_->ShouldShowShelf()) ||
      (web_notification_tray_ &&
       web_notification_tray_->ShouldBlockShelfAutoHide()))
    return true;

  if (palette_tray_ && palette_tray_->ShouldBlockShelfAutoHide())
    return true;

  if (ime_menu_tray_ && ime_menu_tray_->ShouldBlockShelfAutoHide())
    return true;

  return false;
}

bool StatusAreaWidget::IsMessageBubbleShown() const {
  return ((system_tray_ && system_tray_->IsSystemBubbleVisible()) ||
          (web_notification_tray_ &&
           web_notification_tray_->IsMessageCenterBubbleVisible()));
}

void StatusAreaWidget::SchedulePaint() {
  status_area_widget_delegate_->SchedulePaint();
  web_notification_tray_->SchedulePaint();
  system_tray_->SchedulePaint();
  virtual_keyboard_tray_->SchedulePaint();
  logout_button_tray_->SchedulePaint();
  ime_menu_tray_->SchedulePaint();
  if (palette_tray_)
    palette_tray_->SchedulePaint();
  overview_button_tray_->SchedulePaint();
}

const ui::NativeTheme* StatusAreaWidget::GetNativeTheme() const {
  return MaterialDesignController::IsShelfMaterial()
             ? ui::NativeThemeDarkAura::instance()
             : Widget::GetNativeTheme();
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
  logout_button_tray_->UpdateShelfItemBackground(color);
  ime_menu_tray_->UpdateShelfItemBackground(color);
  if (palette_tray_)
    palette_tray_->UpdateShelfItemBackground(color);
  overview_button_tray_->UpdateShelfItemBackground(color);
}

void StatusAreaWidget::AddSystemTray() {
  system_tray_ = new SystemTray(wm_shelf_);
  status_area_widget_delegate_->AddTray(system_tray_);
}

void StatusAreaWidget::AddWebNotificationTray() {
  DCHECK(system_tray_);
  web_notification_tray_ = new WebNotificationTray(
      wm_shelf_, WmWindow::Get(this->GetNativeWindow()), system_tray_);
  status_area_widget_delegate_->AddTray(web_notification_tray_);
}

void StatusAreaWidget::AddLogoutButtonTray() {
  logout_button_tray_ = new LogoutButtonTray(wm_shelf_);
  status_area_widget_delegate_->AddTray(logout_button_tray_);
}

void StatusAreaWidget::AddPaletteTray() {
  const display::Display& display =
      WmWindow::Get(this->GetNativeWindow())->GetDisplayNearestWindow();

  // Create the palette only on the internal display, where the stylus is
  // available. We also create a palette on every display if requested from the
  // command line.
  if (display.IsInternal() || palette_utils::IsPaletteEnabledOnEveryDisplay()) {
    palette_tray_ = new PaletteTray(wm_shelf_);
    status_area_widget_delegate_->AddTray(palette_tray_);
  }
}

void StatusAreaWidget::AddVirtualKeyboardTray() {
  virtual_keyboard_tray_ = new VirtualKeyboardTray(wm_shelf_);
  status_area_widget_delegate_->AddTray(virtual_keyboard_tray_);
}

void StatusAreaWidget::AddImeMenuTray() {
  ime_menu_tray_ = new ImeMenuTray(wm_shelf_);
  status_area_widget_delegate_->AddTray(ime_menu_tray_);
}

void StatusAreaWidget::AddOverviewButtonTray() {
  overview_button_tray_ = new OverviewButtonTray(wm_shelf_);
  status_area_widget_delegate_->AddTray(overview_button_tray_);
}

}  // namespace ash
