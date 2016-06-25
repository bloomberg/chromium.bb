// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/settings/tray_settings.h"

#include "ash/common/session/session_state_delegate.h"
#include "ash/common/system/chromeos/power/power_status.h"
#include "ash/common/system/chromeos/power/power_status_view.h"
#include "ash/common/system/tray/actionable_view.h"
#include "ash/common/system/tray/fixed_sized_image_view.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/wm_shell.h"
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
  explicit SettingsDefaultView(LoginStatus status)
      : login_status_(status), label_(NULL), power_status_view_(NULL) {
    PowerStatus::Get()->AddObserver(this);
    SetLayoutManager(new views::BoxLayout(views::BoxLayout::kHorizontal,
                                          ash::kTrayPopupPaddingHorizontal, 0,
                                          ash::kTrayPopupPaddingBetweenItems));

    bool power_view_right_align = false;
    if (login_status_ != LoginStatus::NOT_LOGGED_IN &&
        login_status_ != LoginStatus::LOCKED &&
        !WmShell::Get()
             ->GetSessionStateDelegate()
             ->IsInSecondaryLoginScreen()) {
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
      power_status_view_ = new ash::PowerStatusView(power_view_right_align);
      AddChildView(power_status_view_);
      OnPowerStatusChanged();
    }
  }

  ~SettingsDefaultView() override { PowerStatus::Get()->RemoveObserver(this); }

  // Overridden from ash::ActionableView.
  bool PerformAction(const ui::Event& event) override {
    if (login_status_ == LoginStatus::NOT_LOGGED_IN ||
        login_status_ == LoginStatus::LOCKED ||
        WmShell::Get()->GetSessionStateDelegate()->IsInSecondaryLoginScreen()) {
      return false;
    }

    WmShell::Get()->system_tray_delegate()->ShowSettings();
    return true;
  }

  // Overridden from views::View.
  void Layout() override {
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
  void ChildPreferredSizeChanged(views::View* child) override {
    views::View::ChildPreferredSizeChanged(child);
    Layout();
  }

  // Overridden from PowerStatus::Observer.
  void OnPowerStatusChanged() override {
    if (!PowerStatus::Get()->IsBatteryPresent())
      return;

    base::string16 accessible_name =
        label_
            ? label_->text() + base::ASCIIToUTF16(", ") +
                  PowerStatus::Get()->GetAccessibleNameString(true)
            : PowerStatus::Get()->GetAccessibleNameString(true);
    SetAccessibleName(accessible_name);
  }

 private:
  LoginStatus login_status_;
  views::Label* label_;
  ash::PowerStatusView* power_status_view_;

  DISALLOW_COPY_AND_ASSIGN(SettingsDefaultView);
};

}  // namespace tray

TraySettings::TraySettings(SystemTray* system_tray)
    : SystemTrayItem(system_tray), default_view_(NULL) {}

TraySettings::~TraySettings() {}

views::View* TraySettings::CreateTrayView(LoginStatus status) {
  return NULL;
}

views::View* TraySettings::CreateDefaultView(LoginStatus status) {
  if ((status == LoginStatus::NOT_LOGGED_IN || status == LoginStatus::LOCKED) &&
      !PowerStatus::Get()->IsBatteryPresent())
    return NULL;
  if (!WmShell::Get()->system_tray_delegate()->ShouldShowSettings())
    return NULL;
  CHECK(default_view_ == NULL);
  default_view_ = new tray::SettingsDefaultView(status);
  return default_view_;
}

views::View* TraySettings::CreateDetailedView(LoginStatus status) {
  NOTIMPLEMENTED();
  return NULL;
}

void TraySettings::DestroyTrayView() {}

void TraySettings::DestroyDefaultView() {
  default_view_ = NULL;
}

void TraySettings::DestroyDetailedView() {}

void TraySettings::UpdateAfterLoginStatusChange(LoginStatus status) {}

}  // namespace ash
