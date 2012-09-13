// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/settings/tray_settings.h"

#include "ash/shell.h"
#include "ash/system/power/power_status_view.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_views.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"

namespace ash {
namespace internal {

namespace tray {

class SettingsDefaultView : public ash::internal::ActionableView {
 public:
  explicit SettingsDefaultView(user::LoginStatus status)
      : login_status_(status),
        label_(NULL),
        power_status_view_(NULL) {
    SetLayoutManager(new views::BoxLayout(views::BoxLayout::kHorizontal,
        ash::kTrayPopupPaddingHorizontal, 0,
        ash::kTrayPopupPaddingBetweenItems));

    bool power_view_right_align = false;
    if (login_status_ != user::LOGGED_IN_NONE &&
        login_status_ != user::LOGGED_IN_LOCKED) {
      ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
      views::ImageView* icon =
          new ash::internal::FixedSizedImageView(0, ash::kTrayPopupItemHeight);
      icon->SetImage(
          rb.GetImageNamed(IDR_AURA_UBER_TRAY_SETTINGS).ToImageSkia());
      AddChildView(icon);

      string16 text = rb.GetLocalizedString(IDS_ASH_STATUS_TRAY_SETTINGS);
      label_ = new views::Label(text);
      AddChildView(label_);
      SetAccessibleName(text);

      power_view_right_align = true;
    }

    PowerSupplyStatus power_status =
        ash::Shell::GetInstance()->tray_delegate()->GetPowerSupplyStatus();
    if (power_status.battery_is_present) {
      power_status_view_ = new ash::internal::PowerStatusView(
          ash::internal::PowerStatusView::VIEW_DEFAULT, power_view_right_align);
      AddChildView(power_status_view_);
      UpdatePowerStatus(power_status);
    }
  }

  virtual ~SettingsDefaultView() {}

  void UpdatePowerStatus(const PowerSupplyStatus& status) {
    if (power_status_view_)
      power_status_view_->UpdatePowerStatus(status);
  }

  // Overridden from ash::internal::ActionableView.
  virtual bool PerformAction(const ui::Event& event) OVERRIDE {
    if (login_status_ == user::LOGGED_IN_NONE ||
        login_status_ == user::LOGGED_IN_LOCKED)
      return false;

    ash::Shell::GetInstance()->tray_delegate()->ShowSettings();
    return true;
  }

  // Overridden from views::View.
  virtual void Layout() OVERRIDE {
    views::View::Layout();

    if (label_ && power_status_view_) {
      // Let the box-layout do the layout first. Then move power_status_view_
      // to right align if it is created.
      gfx::Size size = power_status_view_->GetPreferredSize();
      gfx::Rect bounds(size);
      bounds.set_x(width() - size.width() - ash::kTrayPopupPaddingBetweenItems);
      bounds.set_y((height() - size.height()) / 2);
      power_status_view_->SetBoundsRect(bounds);
    }
  }

  // Overridden from views::View.
  virtual void ChildPreferredSizeChanged(views::View* child) OVERRIDE {
    views::View::ChildPreferredSizeChanged(child);
    Layout();
  }

 private:
  user::LoginStatus login_status_;
  views::Label* label_;
  ash::internal::PowerStatusView* power_status_view_;

  DISALLOW_COPY_AND_ASSIGN(SettingsDefaultView);
 };

}  // namespace tray

TraySettings::TraySettings()
    : default_view_(NULL) {
}

TraySettings::~TraySettings() {}

views::View* TraySettings::CreateTrayView(user::LoginStatus status) {
  return NULL;
}

views::View* TraySettings::CreateDefaultView(user::LoginStatus status) {
  if ((status == user::LOGGED_IN_NONE || status == user::LOGGED_IN_LOCKED) &&
      (!ash::Shell::GetInstance()->tray_delegate()->
          GetPowerSupplyStatus().battery_is_present))
    return NULL;

  CHECK(default_view_ == NULL);
  default_view_ =  new tray::SettingsDefaultView(status);
  return default_view_;
}

views::View* TraySettings::CreateDetailedView(user::LoginStatus status) {
  NOTIMPLEMENTED();
  return NULL;
}

void TraySettings::DestroyTrayView() {
}

void TraySettings::DestroyDefaultView() {
  default_view_ = NULL;
}

void TraySettings::DestroyDetailedView() {
}

void TraySettings::UpdateAfterLoginStatusChange(user::LoginStatus status) {
}

// Overridden from PowerStatusObserver.
void TraySettings::OnPowerStatusChanged(const PowerSupplyStatus& status) {
  if (default_view_)
    default_view_->UpdatePowerStatus(status);
}

}  // namespace internal
}  // namespace ash
