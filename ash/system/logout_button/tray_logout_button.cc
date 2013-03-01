// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/logout_button/tray_logout_button.h"

#include <cstddef>

#include "ash/shell.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/user/login_status.h"
#include "base/logging.h"
#include "base/string16.h"
#include "grit/ash_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace {

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
const int kLogoutButtonHotImages[] = {
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

}  // namespace

namespace ash {
namespace internal {

namespace tray {

class LogoutButton : public views::View,
                     public views::ButtonListener {
 public:
  LogoutButton(user::LoginStatus status) : button_(NULL),
                                           login_status_(user::LOGGED_IN_NONE),
                                           show_logout_button_in_tray_(false) {
    views::BoxLayout* layout = new views::BoxLayout(
        views::BoxLayout::kHorizontal, 0, 0, 0);
    layout->set_spread_blank_space(true);
    SetLayoutManager(layout);
    set_border(views::Border::CreateEmptyBorder(
        0, kTrayLabelItemHorizontalPaddingBottomAlignment, 0, 0));

    button_ = new views::LabelButton(this, string16());
    for (size_t state = 0; state < views::Button::STATE_COUNT; ++state) {
      button_->SetTextColor(
          static_cast<views::Button::ButtonState>(state), SK_ColorWHITE);
    }
    button_->SetFont(button_->GetFont().DeriveFont(1));
    views::LabelButtonBorder* border =
        new views::LabelButtonBorder(views::Button::STYLE_TEXTBUTTON);
    border->SetPainter(false, views::Button::STATE_NORMAL,
        views::Painter::CreateImageGridPainter(kLogoutButtonNormalImages));
    border->SetPainter(false, views::Button::STATE_HOVERED,
        views::Painter::CreateImageGridPainter(kLogoutButtonHotImages));
    border->SetPainter(false, views::Button::STATE_PRESSED,
        views::Painter::CreateImageGridPainter(kLogoutButtonPushedImages));
    button_->set_border(border);
    AddChildView(button_);
    OnLoginStatusChanged(status);
  }

  void OnLoginStatusChanged(user::LoginStatus status) {
    login_status_ = status;
    const string16 title = GetLocalizedSignOutStringForStatus(login_status_,
                                                              false);
    button_->SetText(title);
    button_->SetAccessibleName(title);
    UpdateVisibility();
  }

  void OnShowLogoutButtonInTrayChanged(bool show) {
    show_logout_button_in_tray_ = show;
    UpdateVisibility();
  }

  // Overridden from views::ButtonListener.
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE {
    DCHECK_EQ(sender, button_);
    Shell::GetInstance()->system_tray_delegate()->SignOut();
  }

 private:
  void UpdateVisibility() {
    SetVisible(show_logout_button_in_tray_ &&
               login_status_ != user::LOGGED_IN_NONE &&
               login_status_ != user::LOGGED_IN_LOCKED);
  }

  views::LabelButton* button_;
  user::LoginStatus login_status_;
  bool show_logout_button_in_tray_;

  DISALLOW_COPY_AND_ASSIGN(LogoutButton);
};

}  // namespace tray

TrayLogoutButton::TrayLogoutButton(SystemTray* system_tray)
    : SystemTrayItem(system_tray),
      logout_button_(NULL) {
  Shell::GetInstance()->system_tray_notifier()->AddLogoutButtonObserver(this);
}

TrayLogoutButton::~TrayLogoutButton() {
  Shell::GetInstance()->system_tray_notifier()->
      RemoveLogoutButtonObserver(this);
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
