// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/session/logout_button_tray.h"

#include <memory>
#include <utility>

#include "ash/common/shelf/wm_shelf_util.h"
#include "ash/common/system/chromeos/session/logout_confirmation_controller.h"
#include "ash/common/system/tray/system_tray_controller.h"
#include "ash/common/system/tray/system_tray_notifier.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_utils.h"
#include "ash/common/system/user/login_status.h"
#include "ash/common/wm_shell.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/resources/grit/ash_resources.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "base/logging.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/events/event.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/bubble/tray_bubble_view.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/painter.h"

namespace ash {

LogoutButtonTray::LogoutButtonTray(WmShelf* wm_shelf)
    : TrayBackgroundView(wm_shelf),
      button_(nullptr),
      login_status_(LoginStatus::NOT_LOGGED_IN),
      show_logout_button_in_tray_(false) {
  views::MdTextButton* button =
      views::MdTextButton::Create(this, base::string16());
  button->SetProminent(true);
  button->SetBgColorOverride(gfx::kGoogleRed700);
  // Base font size + 2 = 14.
  // TODO(estade): should this 2 be shared with other tray views? See
  // crbug.com/623987
  button->AdjustFontSize(2);
  button_ = button;

  // Since LogoutButtonTray has a red background and it is distinguished
  // by itself, no separator is needed on its right side.
  set_separator_visibility(false);
  tray_container()->AddChildView(button_);
  WmShell::Get()->system_tray_notifier()->AddLogoutButtonObserver(this);
}

LogoutButtonTray::~LogoutButtonTray() {
  WmShell::Get()->system_tray_notifier()->RemoveLogoutButtonObserver(this);
}

void LogoutButtonTray::SetShelfAlignment(ShelfAlignment alignment) {
  // We must first update the button so that
  // TrayBackgroundView::SetShelfAlignment() can lay it out correctly.
  UpdateButtonTextAndImage(login_status_, alignment);
  TrayBackgroundView::SetShelfAlignment(alignment);
}

base::string16 LogoutButtonTray::GetAccessibleNameForTray() {
  return button_->GetText();
}

void LogoutButtonTray::HideBubbleWithView(
    const views::TrayBubbleView* bubble_view) {}

void LogoutButtonTray::ClickedOutsideBubble() {}

void LogoutButtonTray::ButtonPressed(views::Button* sender,
                                     const ui::Event& event) {
  if (sender != button_) {
    TrayBackgroundView::ButtonPressed(sender, event);
    return;
  }

  if (dialog_duration_ <= base::TimeDelta()) {
    // Sign out immediately if |dialog_duration_| is non-positive.
    WmShell::Get()->system_tray_controller()->SignOut();
  } else if (WmShell::Get()->logout_confirmation_controller()) {
    WmShell::Get()->logout_confirmation_controller()->ConfirmLogout(
        base::TimeTicks::Now() + dialog_duration_);
  }
}

void LogoutButtonTray::OnShowLogoutButtonInTrayChanged(bool show) {
  show_logout_button_in_tray_ = show;
  UpdateVisibility();
}

void LogoutButtonTray::OnLogoutDialogDurationChanged(base::TimeDelta duration) {
  dialog_duration_ = duration;
}

void LogoutButtonTray::UpdateAfterLoginStatusChange(LoginStatus login_status) {
  UpdateButtonTextAndImage(login_status, shelf_alignment());
}

void LogoutButtonTray::UpdateVisibility() {
  SetVisible(show_logout_button_in_tray_ &&
             login_status_ != LoginStatus::NOT_LOGGED_IN &&
             login_status_ != LoginStatus::LOCKED);
}

void LogoutButtonTray::UpdateButtonTextAndImage(LoginStatus login_status,
                                                ShelfAlignment alignment) {
  login_status_ = login_status;
  const base::string16 title =
      user::GetLocalizedSignOutStringForStatus(login_status, false);
  if (IsHorizontalAlignment(alignment)) {
    button_->SetText(title);
    button_->SetImage(views::LabelButton::STATE_NORMAL, gfx::ImageSkia());
    button_->SetMinSize(gfx::Size(0, kTrayItemSize));
  } else {
    button_->SetText(base::string16());
    button_->SetAccessibleName(title);
    button_->SetImage(views::LabelButton::STATE_NORMAL,
                      gfx::CreateVectorIcon(kShelfLogoutIcon, kTrayIconColor));
    button_->SetMinSize(gfx::Size(kTrayItemSize, kTrayItemSize));
  }
  UpdateVisibility();
}

}  // namespace ash
