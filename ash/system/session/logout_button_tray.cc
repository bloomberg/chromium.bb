// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/session/logout_button_tray.h"

#include <memory>

#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/ash_typography.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/session/logout_confirmation_controller.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/user/login_status.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

LogoutButtonTray::LogoutButtonTray(Shelf* shelf)
    : shelf_(shelf),
      container_(new TrayContainer(shelf)),
      button_(views::MdTextButton::Create(this,
                                          base::string16(),
                                          CONTEXT_LAUNCHER_BUTTON)),
      show_logout_button_in_tray_(false) {
  DCHECK(shelf);
  Shell::Get()->session_controller()->AddObserver(this);
  SetLayoutManager(std::make_unique<views::FillLayout>());
  AddChildView(container_);

  button_->SetProminent(true);
  button_->SetBgColorOverride(gfx::kGoogleRed700);

  container_->AddChildView(button_);
  SetVisible(false);
}

LogoutButtonTray::~LogoutButtonTray() {
  Shell::Get()->session_controller()->RemoveObserver(this);
}

// static
void LogoutButtonTray::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kShowLogoutButtonInTray, false,
                                PrefRegistry::PUBLIC);
  registry->RegisterIntegerPref(prefs::kLogoutDialogDurationMs, 20000,
                                PrefRegistry::PUBLIC);
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
    Shell::Get()->session_controller()->RequestSignOut();
  } else if (Shell::Get()->logout_confirmation_controller()) {
    Shell::Get()->logout_confirmation_controller()->ConfirmLogout(
        base::TimeTicks::Now() + dialog_duration_);
  }
}

void LogoutButtonTray::OnActiveUserPrefServiceChanged(PrefService* prefs) {
  pref_change_registrar_.reset();
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(prefs);
  pref_change_registrar_->Add(
      prefs::kShowLogoutButtonInTray,
      base::Bind(&LogoutButtonTray::UpdateShowLogoutButtonInTray,
                 base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kLogoutDialogDurationMs,
      base::Bind(&LogoutButtonTray::UpdateLogoutDialogDuration,
                 base::Unretained(this)));

  // Read the initial values.
  UpdateShowLogoutButtonInTray();
  UpdateLogoutDialogDuration();
}

void LogoutButtonTray::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  View::GetAccessibleNodeData(node_data);
  node_data->SetName(button_->GetText());
}

void LogoutButtonTray::UpdateShowLogoutButtonInTray() {
  show_logout_button_in_tray_ = pref_change_registrar_->prefs()->GetBoolean(
      prefs::kShowLogoutButtonInTray);
  UpdateVisibility();
}

void LogoutButtonTray::UpdateLogoutDialogDuration() {
  const int duration_ms = pref_change_registrar_->prefs()->GetInteger(
      prefs::kLogoutDialogDurationMs);
  dialog_duration_ = base::TimeDelta::FromMilliseconds(duration_ms);
}

void LogoutButtonTray::UpdateAfterLoginStatusChange() {
  UpdateButtonTextAndImage();
}

void LogoutButtonTray::UpdateVisibility() {
  LoginStatus login_status = shelf_->GetStatusAreaWidget()->login_status();
  SetVisible(show_logout_button_in_tray_ &&
             login_status != LoginStatus::NOT_LOGGED_IN &&
             login_status != LoginStatus::LOCKED);
}

void LogoutButtonTray::UpdateButtonTextAndImage() {
  LoginStatus login_status = shelf_->GetStatusAreaWidget()->login_status();
  const base::string16 title =
      user::GetLocalizedSignOutStringForStatus(login_status, false);
  if (shelf_->IsHorizontalAlignment()) {
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
