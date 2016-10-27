// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/session/logout_button_tray.h"

#include <memory>
#include <utility>

#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/system/chromeos/session/logout_confirmation_controller.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/system/tray/system_tray_notifier.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_utils.h"
#include "ash/common/system/user/login_status.h"
#include "ash/common/wm_shell.h"
#include "ash/public/cpp/shelf_types.h"
#include "base/logging.h"
#include "grit/ash_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/events/event.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icons_public.h"
#include "ui/views/bubble/tray_bubble_view.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/button/md_text_button.h"
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
    IDR_AURA_UBER_TRAY_LOGOUT_BUTTON_NORMAL_BOTTOM_RIGHT};

const int kLogoutButtonPushedImages[] = {
    IDR_AURA_UBER_TRAY_LOGOUT_BUTTON_PUSHED_TOP_LEFT,
    IDR_AURA_UBER_TRAY_LOGOUT_BUTTON_PUSHED_TOP,
    IDR_AURA_UBER_TRAY_LOGOUT_BUTTON_PUSHED_TOP_RIGHT,
    IDR_AURA_UBER_TRAY_LOGOUT_BUTTON_PUSHED_LEFT,
    IDR_AURA_UBER_TRAY_LOGOUT_BUTTON_PUSHED_CENTER,
    IDR_AURA_UBER_TRAY_LOGOUT_BUTTON_PUSHED_RIGHT,
    IDR_AURA_UBER_TRAY_LOGOUT_BUTTON_PUSHED_BOTTOM_LEFT,
    IDR_AURA_UBER_TRAY_LOGOUT_BUTTON_PUSHED_BOTTOM,
    IDR_AURA_UBER_TRAY_LOGOUT_BUTTON_PUSHED_BOTTOM_RIGHT};

// TODO(estade): LogoutButton is not used in MD; remove it when possible.
// See crbug.com/614453
class LogoutButton : public views::LabelButton {
 public:
  LogoutButton(views::ButtonListener* listener);
  ~LogoutButton() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(LogoutButton);
};

}  // namespace

LogoutButton::LogoutButton(views::ButtonListener* listener)
    : views::LabelButton(listener, base::string16()) {
  SetupLabelForTray(label());
  SetFontList(label()->font_list());
  SetEnabledTextColors(SK_ColorWHITE);

  std::unique_ptr<views::LabelButtonAssetBorder> border(
      new views::LabelButtonAssetBorder(views::Button::STYLE_TEXTBUTTON));
  border->SetPainter(
      false, views::Button::STATE_NORMAL,
      views::Painter::CreateImageGridPainter(kLogoutButtonNormalImages));
  border->SetPainter(
      false, views::Button::STATE_HOVERED,
      views::Painter::CreateImageGridPainter(kLogoutButtonNormalImages));
  border->SetPainter(
      false, views::Button::STATE_PRESSED,
      views::Painter::CreateImageGridPainter(kLogoutButtonPushedImages));
  gfx::Insets insets = border->GetInsets();
  insets += gfx::Insets(0, kLogoutButtonHorizontalExtraPadding, 0,
                        kLogoutButtonHorizontalExtraPadding);
  border->set_insets(insets);
  SetBorder(std::move(border));
  set_animate_on_state_change(false);

  SetMinSize(gfx::Size(0, GetTrayConstant(TRAY_ITEM_HEIGHT_LEGACY)));
}

LogoutButton::~LogoutButton() {}

LogoutButtonTray::LogoutButtonTray(WmShelf* wm_shelf)
    : TrayBackgroundView(wm_shelf),
      button_(nullptr),
      login_status_(LoginStatus::NOT_LOGGED_IN),
      show_logout_button_in_tray_(false) {
  if (MaterialDesignController::IsShelfMaterial()) {
    views::MdTextButton* button =
        views::MdTextButton::Create(this, base::string16());
    button->SetProminent(true);
    button->SetBgColorOverride(gfx::kGoogleRed700);
    // Base font size + 2 = 14.
    // TODO(estade): should this 2 be shared with other tray views? See
    // crbug.com/623987
    button->AdjustFontSize(2);
    button_ = button;
  } else {
    button_ = new LogoutButton(this);
  }
  tray_container()->AddChildView(button_);
  if (!MaterialDesignController::IsShelfMaterial())
    tray_container()->SetBorder(views::Border::NullBorder());
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
  if (!MaterialDesignController::IsShelfMaterial())
    tray_container()->SetBorder(views::Border::NullBorder());
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
    WmShell::Get()->system_tray_delegate()->SignOut();
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
  const int button_size = MaterialDesignController::IsShelfMaterial()
                              ? kTrayItemSize
                              : GetTrayConstant(TRAY_ITEM_HEIGHT_LEGACY);
  if (alignment == SHELF_ALIGNMENT_BOTTOM) {
    button_->SetText(title);
    button_->SetImage(views::LabelButton::STATE_NORMAL, gfx::ImageSkia());
    button_->SetMinSize(gfx::Size(0, button_size));
  } else {
    button_->SetText(base::string16());
    button_->SetAccessibleName(title);
    button_->SetImage(
        views::LabelButton::STATE_NORMAL,
        gfx::CreateVectorIcon(gfx::VectorIconId::SHELF_LOGOUT, kTrayIconColor));
    button_->SetMinSize(gfx::Size(button_size, button_size));
  }
  UpdateVisibility();
}

}  // namespace ash
