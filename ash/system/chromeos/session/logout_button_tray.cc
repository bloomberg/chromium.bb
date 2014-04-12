// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/session/logout_button_tray.h"

#include "ash/shelf/shelf_types.h"
#include "ash/shell.h"
#include "ash/system/chromeos/session/logout_confirmation_controller.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_utils.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "grit/ash_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/bubble/tray_bubble_view.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/painter.h"

namespace ash {
namespace {

const int kLogoutButtonHorizontalExtraPadding = 7;

const int kLogoutButtonNormalImages[] = {
  IDR_AURA_UBER_TRAY_LOGOUT_BUTTON_NORMAL_TOP_LEFT,
  IDR_AURA_UBER_TRAY_LOGOUT_BUTTON_NORMAL_TOP,
  IDR_AURA_UBER_TRAY_LOGOUT_BUTTON_NORMAL_TOP_RIGHT,
  IDR_AURA_UBER_TRAY_LOGOUT_BUTTON_NORMAL_LEFT,
  IDR_AURA_UBER_TRAY_LOGOUT_BUTTON_NORMAL_CENTER,
  IDR_AURA_UBER_TRAY_LOGOUT_BUTTON_NORMAL_RIGHT,
  IDR_AURA_UBER_TRAY_LOGOUT_BUTTON_NORMAL_BOTTOM_LEFT,
  IDR_AURA_UBER_TRAY_LOGOUT_BUTTON_NORMAL_BOTTOM,
  IDR_AURA_UBER_TRAY_LOGOUT_BUTTON_NORMAL_BOTTOM_RIGHT
};

const int kLogoutButtonPushedImages[] = {
  IDR_AURA_UBER_TRAY_LOGOUT_BUTTON_PUSHED_TOP_LEFT,
  IDR_AURA_UBER_TRAY_LOGOUT_BUTTON_PUSHED_TOP,
  IDR_AURA_UBER_TRAY_LOGOUT_BUTTON_PUSHED_TOP_RIGHT,
  IDR_AURA_UBER_TRAY_LOGOUT_BUTTON_PUSHED_LEFT,
  IDR_AURA_UBER_TRAY_LOGOUT_BUTTON_PUSHED_CENTER,
  IDR_AURA_UBER_TRAY_LOGOUT_BUTTON_PUSHED_RIGHT,
  IDR_AURA_UBER_TRAY_LOGOUT_BUTTON_PUSHED_BOTTOM_LEFT,
  IDR_AURA_UBER_TRAY_LOGOUT_BUTTON_PUSHED_BOTTOM,
  IDR_AURA_UBER_TRAY_LOGOUT_BUTTON_PUSHED_BOTTOM_RIGHT
};

class LogoutButton : public views::LabelButton {
 public:
  LogoutButton(views::ButtonListener* listener);
  virtual ~LogoutButton();

 private:
  DISALLOW_COPY_AND_ASSIGN(LogoutButton);
};

}  // namespace

LogoutButton::LogoutButton(views::ButtonListener* listener)
    : views::LabelButton(listener, base::string16()) {
  SetupLabelForTray(label());
  SetFontList(label()->font_list());
  for (size_t state = 0; state < views::Button::STATE_COUNT; ++state)
    SetTextColor(static_cast<views::Button::ButtonState>(state), SK_ColorWHITE);

  scoped_ptr<views::LabelButtonBorder> border(
      new views::LabelButtonBorder(views::Button::STYLE_TEXTBUTTON));
  border->SetPainter(false, views::Button::STATE_NORMAL,
      views::Painter::CreateImageGridPainter(kLogoutButtonNormalImages));
  border->SetPainter(false, views::Button::STATE_HOVERED,
      views::Painter::CreateImageGridPainter(kLogoutButtonNormalImages));
  border->SetPainter(false, views::Button::STATE_PRESSED,
      views::Painter::CreateImageGridPainter(kLogoutButtonPushedImages));
  gfx::Insets insets = border->GetInsets();
  insets += gfx::Insets(0, kLogoutButtonHorizontalExtraPadding,
                        0, kLogoutButtonHorizontalExtraPadding);
  border->set_insets(insets);
  SetBorder(border.PassAs<views::Border>());
  set_animate_on_state_change(false);

  set_min_size(gfx::Size(0, kShelfItemHeight));
}

LogoutButton::~LogoutButton() {
}

LogoutButtonTray::LogoutButtonTray(StatusAreaWidget* status_area_widget)
    : TrayBackgroundView(status_area_widget),
      button_(NULL),
      login_status_(user::LOGGED_IN_NONE),
      show_logout_button_in_tray_(false) {
  button_ = new LogoutButton(this);
  tray_container()->AddChildView(button_);
  tray_container()->SetBorder(views::Border::NullBorder());
  Shell::GetInstance()->system_tray_notifier()->AddLogoutButtonObserver(this);
}

LogoutButtonTray::~LogoutButtonTray() {
  Shell::GetInstance()->system_tray_notifier()->
      RemoveLogoutButtonObserver(this);
}

void LogoutButtonTray::SetShelfAlignment(ShelfAlignment alignment) {
  TrayBackgroundView::SetShelfAlignment(alignment);
  tray_container()->SetBorder(views::Border::NullBorder());
}

base::string16 LogoutButtonTray::GetAccessibleNameForTray() {
  return button_->GetText();
}

void LogoutButtonTray::HideBubbleWithView(
    const views::TrayBubbleView* bubble_view) {
}

bool LogoutButtonTray::ClickedOutsideBubble() {
  return false;
}

void LogoutButtonTray::OnShowLogoutButtonInTrayChanged(bool show) {
  show_logout_button_in_tray_ = show;
  UpdateVisibility();
}

void LogoutButtonTray::OnLogoutDialogDurationChanged(base::TimeDelta duration) {
  dialog_duration_ = duration;
}

void LogoutButtonTray::ButtonPressed(views::Button* sender,
                                     const ui::Event& event) {
  DCHECK_EQ(sender, button_);
  if (dialog_duration_ <= base::TimeDelta()) {
    // Sign out immediately if |dialog_duration_| is non-positive.
    Shell::GetInstance()->system_tray_delegate()->SignOut();
  } else if (Shell::GetInstance()->logout_confirmation_controller()) {
    Shell::GetInstance()->logout_confirmation_controller()->ConfirmLogout(
        base::TimeTicks::Now() + dialog_duration_);
  }
}

void LogoutButtonTray::UpdateAfterLoginStatusChange(
    user::LoginStatus login_status) {
  login_status_ = login_status;
  const base::string16 title =
      GetLocalizedSignOutStringForStatus(login_status, false);
  button_->SetText(title);
  button_->SetAccessibleName(title);
  UpdateVisibility();
}

void LogoutButtonTray::UpdateVisibility() {
  SetVisible(show_logout_button_in_tray_ &&
             login_status_ != user::LOGGED_IN_NONE &&
             login_status_ != user::LOGGED_IN_LOCKED);
}

}  // namespace ash
