// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/settings/tray_settings.h"

#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/session/session_state_delegate.h"
#include "ash/common/system/chromeos/power/power_status.h"
#include "ash/common/system/chromeos/power/power_status_view.h"
#include "ash/common/system/tray/actionable_view.h"
#include "ash/common/system/tray/fixed_sized_image_view.h"
#include "ash/common/system/tray/system_tray_controller.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_popup_utils.h"
#include "ash/common/wm_shell.h"
#include "ash/resources/grit/ash_resources.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
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

// TODO(tdanderson): Remove this class once material design is enabled by
// default. See crbug.com/614453.
class SettingsDefaultView : public ActionableView,
                            public PowerStatus::Observer {
 public:
  SettingsDefaultView(SystemTrayItem* owner, LoginStatus status)
      : ActionableView(owner, TrayPopupInkDropStyle::FILL_BOUNDS),
        login_status_(status),
        label_(nullptr),
        power_status_view_(nullptr) {
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
      views::ImageView* icon = TrayPopupUtils::CreateMainImageView();

      icon->SetImage(
          rb.GetImageNamed(IDR_AURA_UBER_TRAY_SETTINGS).ToImageSkia());
      icon->set_id(test::kSettingsTrayItemViewId);
      AddChildView(icon);

      base::string16 text = rb.GetLocalizedString(IDS_ASH_STATUS_TRAY_SETTINGS);
      label_ = TrayPopupUtils::CreateDefaultLabel();
      label_->SetText(text);
      AddChildView(label_);
      SetAccessibleName(text);

      power_view_right_align = true;
    }

    if (PowerStatus::Get()->IsBatteryPresent()) {
      power_status_view_ = new ash::PowerStatusView(power_view_right_align);
      AddChildView(power_status_view_);
      OnPowerStatusChanged();
    }

    if (MaterialDesignController::IsSystemTrayMenuMaterial())
      SetInkDropMode(InkDropHostView::InkDropMode::ON);
  }

  ~SettingsDefaultView() override { PowerStatus::Get()->RemoveObserver(this); }

  // Overridden from ash::ActionableView.
  bool PerformAction(const ui::Event& event) override {
    if (login_status_ == LoginStatus::NOT_LOGGED_IN ||
        login_status_ == LoginStatus::LOCKED ||
        WmShell::Get()->GetSessionStateDelegate()->IsInSecondaryLoginScreen()) {
      return false;
    }

    WmShell::Get()->system_tray_controller()->ShowSettings();
    CloseSystemBubble();
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
    : SystemTrayItem(system_tray, UMA_SETTINGS), default_view_(nullptr) {}

TraySettings::~TraySettings() {}

views::View* TraySettings::CreateTrayView(LoginStatus status) {
  return nullptr;
}

views::View* TraySettings::CreateDefaultView(LoginStatus status) {
  if ((status == LoginStatus::NOT_LOGGED_IN || status == LoginStatus::LOCKED) &&
      !PowerStatus::Get()->IsBatteryPresent())
    return nullptr;
  if (!WmShell::Get()->system_tray_delegate()->ShouldShowSettings())
    return nullptr;
  CHECK(default_view_ == nullptr);
  default_view_ = new tray::SettingsDefaultView(this, status);
  return default_view_;
}

views::View* TraySettings::CreateDetailedView(LoginStatus status) {
  NOTIMPLEMENTED();
  return nullptr;
}

void TraySettings::DestroyTrayView() {}

void TraySettings::DestroyDefaultView() {
  default_view_ = nullptr;
}

void TraySettings::DestroyDetailedView() {}

void TraySettings::UpdateAfterLoginStatusChange(LoginStatus status) {}

}  // namespace ash
