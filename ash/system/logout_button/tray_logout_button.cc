// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/logout_button/tray_logout_button.h"

#include <cstddef>

#include "ash/shell.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/user/login_status.h"
#include "base/logging.h"
#include "base/string16.h"
#include "grit/ash_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/controls/button/border_images.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/label_button_border.h"

namespace {

const int kLogoutButtonBorderNormalImages[] = {
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
const int kLogoutButtonBorderHotImages[] = {
  IDR_AURA_UBER_TRAY_LOGOUT_BUTTON_HOT_TOP_LEFT,
  IDR_AURA_UBER_TRAY_LOGOUT_BUTTON_HOT_TOP,
  IDR_AURA_UBER_TRAY_LOGOUT_BUTTON_HOT_TOP_RIGHT,
  IDR_AURA_UBER_TRAY_LOGOUT_BUTTON_HOT_LEFT,
  IDR_AURA_UBER_TRAY_LOGOUT_BUTTON_HOT_CENTER,
  IDR_AURA_UBER_TRAY_LOGOUT_BUTTON_HOT_RIGHT,
  IDR_AURA_UBER_TRAY_LOGOUT_BUTTON_HOT_BOTTOM_LEFT,
  IDR_AURA_UBER_TRAY_LOGOUT_BUTTON_HOT_BOTTOM,
  IDR_AURA_UBER_TRAY_LOGOUT_BUTTON_HOT_BOTTOM_RIGHT
};
const int kLogoutButtonBorderPushedImages[] = {
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

}  // namespace

namespace ash {
namespace internal {

namespace tray {

class LogoutButton : public views::LabelButton,
                     public views::ButtonListener {
 public:
  LogoutButton(user::LoginStatus status) : views::LabelButton(this, string16()),
                                           login_status_(user::LOGGED_IN_NONE),
                                           show_logout_button_in_tray_(false) {
    for (size_t state = 0; state < views::CustomButton::STATE_COUNT; ++state) {
      SetTextColor(static_cast<views::CustomButton::ButtonState>(state),
                   SK_ColorWHITE);
    }
    SetFont(GetFont().DeriveFont(1));
    views::LabelButtonBorder* border = new views::LabelButtonBorder();
    border->SetImages(views::CustomButton::STATE_NORMAL,
                      views::BorderImages(kLogoutButtonBorderNormalImages));
    border->SetImages(views::CustomButton::STATE_HOVERED,
                      views::BorderImages(kLogoutButtonBorderHotImages));
    border->SetImages(views::CustomButton::STATE_PRESSED,
                      views::BorderImages(kLogoutButtonBorderPushedImages));
    set_border(border);
    OnLoginStatusChanged(status);
  }

  void OnLoginStatusChanged(user::LoginStatus status) {
    login_status_ = status;
    const string16 title = GetLocalizedSignOutStringForStatus(login_status_,
                                                              false);
    SetText(title);
    SetAccessibleName(title);
    UpdateVisibility();
  }

  void OnShowLogoutButtonInTrayChanged(bool show) {
    show_logout_button_in_tray_ = show;
    UpdateVisibility();
  }

  // Overridden from views::ButtonListener.
  void ButtonPressed(views::Button* sender, const ui::Event& event) OVERRIDE {
    DCHECK_EQ(sender, this);
    Shell::GetInstance()->tray_delegate()->SignOut();
  }

 private:
  void UpdateVisibility() {
    SetVisible(show_logout_button_in_tray_ &&
               login_status_ != user::LOGGED_IN_NONE &&
               login_status_ != user::LOGGED_IN_LOCKED);
  }

  user::LoginStatus login_status_;
  bool show_logout_button_in_tray_;

  DISALLOW_COPY_AND_ASSIGN(LogoutButton);
};

}  // namespace tray

TrayLogoutButton::TrayLogoutButton(SystemTray* system_tray)
    : SystemTrayItem(system_tray),
      logout_button_(NULL) {
}

views::View* TrayLogoutButton::CreateTrayView(user::LoginStatus status) {
  CHECK(logout_button_ == NULL);
  logout_button_ = new tray::LogoutButton(status);
  return logout_button_;
}

void TrayLogoutButton::DestroyTrayView() {
  logout_button_ = NULL;
}

void TrayLogoutButton::UpdateAfterLoginStatusChange(user::LoginStatus status) {
  logout_button_->OnLoginStatusChanged(status);
}

void TrayLogoutButton::OnShowLogoutButtonInTrayChanged(bool show) {
  logout_button_->OnShowLogoutButtonInTrayChanged(show);
}

}  // namespace internal
}  // namespace ash
