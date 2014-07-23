// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/settings/tray_settings.h"

#include "ash/shell.h"
#include "ash/system/chromeos/power/power_status.h"
#include "ash/system/chromeos/power/power_status_view.h"
#include "ash/system/tray/actionable_view.h"
#include "ash/system/tray/fixed_sized_image_view.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/tray_constants.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"

namespace ash {
namespace tray {

class SettingsDefaultView : public ActionableView,
                            public PowerStatus::Observer {
 public:
  explicit SettingsDefaultView(user::LoginStatus status)
      : login_status_(status),
        label_(NULL),
        power_status_view_(NULL) {
    PowerStatus::Get()->AddObserver(this);
    SetLayoutManager(new views::BoxLayout(views::BoxLayout::kHorizontal,
        ash::kTrayPopupPaddingHorizontal, 0,
        ash::kTrayPopupPaddingBetweenItems));

    bool power_view_right_align = false;
    if (login_status_ != user::LOGGED_IN_NONE &&
        login_status_ != user::LOGGED_IN_LOCKED) {
      ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
      views::ImageView* icon =
          new ash::FixedSizedImageView(0, ash::kTrayPopupItemHeight);
      icon->SetImage(
          rb.GetImageNamed(IDR_AURA_UBER_TRAY_SETTINGS).ToImageSkia());
      icon->set_id(test::kSettingsTrayItemViewId);
      AddChildView(icon);

      base::string16 text = rb.GetLocalizedString(IDS_ASH_STATUS_TRAY_SETTINGS);
      label_ = new views::Label(text);
      AddChildView(label_);
      SetAccessibleName(text);

      power_view_right_align = true;
    }

    if (PowerStatus::Get()->IsBatteryPresent()) {
      power_status_view_ = new ash::PowerStatusView(
          ash::PowerStatusView::VIEW_DEFAULT, power_view_right_align);
      AddChildView(power_status_view_);
      OnPowerStatusChanged();
    }
  }

  virtual ~SettingsDefaultView() {
    PowerStatus::Get()->RemoveObserver(this);
  }

  // Overridden from ash::ActionableView.
  virtual bool PerformAction(const ui::Event& event) OVERRIDE {
    if (login_status_ == user::LOGGED_IN_NONE ||
        login_status_ == user::LOGGED_IN_LOCKED)
      return false;

    ash::Shell::GetInstance()->system_tray_delegate()->ShowSettings();
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

  // Overridden from PowerStatus::Observer.
  virtual void OnPowerStatusChanged() OVERRIDE {
    if (!PowerStatus::Get()->IsBatteryPresent())
      return;

    base::string16 accessible_name = label_ ?
        label_->text() + base::ASCIIToUTF16(", ") +
            PowerStatus::Get()->GetAccessibleNameString(true) :
        PowerStatus::Get()->GetAccessibleNameString(true);
    SetAccessibleName(accessible_name);
  }

 private:
  user::LoginStatus login_status_;
  views::Label* label_;
  ash::PowerStatusView* power_status_view_;

  DISALLOW_COPY_AND_ASSIGN(SettingsDefaultView);
 };

}  // namespace tray

TraySettings::TraySettings(SystemTray* system_tray)
    : SystemTrayItem(system_tray),
      default_view_(NULL) {
}

TraySettings::~TraySettings() {
}

views::View* TraySettings::CreateTrayView(user::LoginStatus status) {
  return NULL;
}

views::View* TraySettings::CreateDefaultView(user::LoginStatus status) {
  if ((status == user::LOGGED_IN_NONE || status == user::LOGGED_IN_LOCKED) &&
      !PowerStatus::Get()->IsBatteryPresent())
    return NULL;
  if (!ash::Shell::GetInstance()->system_tray_delegate()->ShouldShowSettings())
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

}  // namespace ash
