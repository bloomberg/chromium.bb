// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/session/logout_button_tray.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shelf/wm_shelf.h"
#include "ash/shell.h"
#include "ash/system/session/logout_confirmation_controller.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/system_tray_controller.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/user/login_status.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

LogoutButtonTray::LogoutButtonTray(WmShelf* wm_shelf)
    : wm_shelf_(wm_shelf),
      container_(new TrayContainer(wm_shelf)),
      button_(views::MdTextButton::Create(this, base::string16())),
      show_logout_button_in_tray_(false) {
  SetLayoutManager(new views::FillLayout);
  AddChildView(container_);

  button_->SetProminent(true);
  button_->SetBgColorOverride(gfx::kGoogleRed700);
  button_->AdjustFontSize(kTrayTextFontSizeIncrease);

  container_->AddChildView(button_);
  Shell::Get()->system_tray_notifier()->AddLogoutButtonObserver(this);
  SetVisible(false);
}

LogoutButtonTray::~LogoutButtonTray() {
  Shell::Get()->system_tray_notifier()->RemoveLogoutButtonObserver(this);
}

void LogoutButtonTray::UpdateAfterShelfAlignmentChange() {
  // We must first update the button so that |container_| can lay it out
  // correctly.
  UpdateButtonTextAndImage();
  container_->UpdateAfterShelfAlignmentChange();
}

void LogoutButtonTray::ButtonPressed(views::Button* sender,
                                     const ui::Event& event) {
  DCHECK_EQ(button_, sender);

  if (dialog_duration_ <= base::TimeDelta()) {
    // Sign out immediately if |dialog_duration_| is non-positive.
    Shell::Get()->system_tray_controller()->SignOut();
  } else if (Shell::Get()->logout_confirmation_controller()) {
    Shell::Get()->logout_confirmation_controller()->ConfirmLogout(
        base::TimeTicks::Now() + dialog_duration_);
  }
}

void LogoutButtonTray::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  View::GetAccessibleNodeData(node_data);
  node_data->SetName(button_->GetText());
}

void LogoutButtonTray::OnShowLogoutButtonInTrayChanged(bool show) {
  show_logout_button_in_tray_ = show;
  UpdateVisibility();
}

void LogoutButtonTray::OnLogoutDialogDurationChanged(base::TimeDelta duration) {
  dialog_duration_ = duration;
}

void LogoutButtonTray::UpdateAfterLoginStatusChange() {
  UpdateButtonTextAndImage();
}

void LogoutButtonTray::UpdateVisibility() {
  LoginStatus login_status = wm_shelf_->GetStatusAreaWidget()->login_status();
  SetVisible(show_logout_button_in_tray_ &&
             login_status != LoginStatus::NOT_LOGGED_IN &&
             login_status != LoginStatus::LOCKED);
}

void LogoutButtonTray::UpdateButtonTextAndImage() {
  LoginStatus login_status = wm_shelf_->GetStatusAreaWidget()->login_status();
  const base::string16 title =
      user::GetLocalizedSignOutStringForStatus(login_status, false);
  if (wm_shelf_->IsHorizontalAlignment()) {
    button_->SetText(title);
    button_->SetImage(views::Button::STATE_NORMAL, gfx::ImageSkia());
    button_->SetMinSize(gfx::Size(0, kTrayItemSize));
  } else {
    button_->SetText(base::string16());
    button_->SetAccessibleName(title);
    button_->SetImage(views::Button::STATE_NORMAL,
                      gfx::CreateVectorIcon(kShelfLogoutIcon, kTrayIconColor));
    button_->SetMinSize(gfx::Size(kTrayItemSize, kTrayItemSize));
  }
  UpdateVisibility();
}

}  // namespace ash
