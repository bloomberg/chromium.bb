// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/power/power_status_view.h"

#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/system/chromeos/power/power_status.h"
#include "ash/common/system/chromeos/power/tray_power.h"
#include "ash/common/system/tray/fixed_sized_image_view.h"
#include "ash/common/system/tray/tray_constants.h"
#include "base/i18n/message_formatter.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/utf_string_conversions.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"

namespace ash {

// Padding between battery status text and battery icon on default view.
const int kPaddingBetweenBatteryStatusAndIcon = 3;

PowerStatusView::PowerStatusView(bool default_view_right_align)
    : default_view_right_align_(default_view_right_align),
      percentage_label_(new views::Label),
      separator_label_(new views::Label),
      time_status_label_(new views::Label),
      icon_(nullptr) {
  percentage_label_->SetEnabledColor(kHeaderTextColorNormal);
  separator_label_->SetEnabledColor(kHeaderTextColorNormal);
  separator_label_->SetText(base::ASCIIToUTF16(" - "));
  LayoutView();
  PowerStatus::Get()->AddObserver(this);
  OnPowerStatusChanged();
}

PowerStatusView::~PowerStatusView() {
  PowerStatus::Get()->RemoveObserver(this);
}

void PowerStatusView::OnPowerStatusChanged() {
  UpdateText();

  // We do not show a battery icon in the material design system menu.
  // TODO(tdanderson): Remove the non-MD code and the IconSet enum once
  // material design is enabled by default. See crbug.com/614453.
  if (MaterialDesignController::UseMaterialDesignSystemIcons())
    return;

  const PowerStatus::BatteryImageInfo info =
      PowerStatus::Get()->GetBatteryImageInfo(PowerStatus::ICON_DARK);
  if (info != previous_battery_image_info_) {
    icon_->SetImage(PowerStatus::Get()->GetBatteryImage(info));
    icon_->SetVisible(true);
    previous_battery_image_info_ = info;
  }
}

void PowerStatusView::LayoutView() {
  if (default_view_right_align_) {
    views::BoxLayout* layout =
        new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0,
                             kPaddingBetweenBatteryStatusAndIcon);
    SetLayoutManager(layout);

    AddChildView(percentage_label_);
    AddChildView(separator_label_);
    AddChildView(time_status_label_);

    icon_ = new views::ImageView;
    AddChildView(icon_);
  } else {
    // PowerStatusView is left aligned on the system tray pop up item.
    views::BoxLayout* layout = new views::BoxLayout(
        views::BoxLayout::kHorizontal, 0, 0, kTrayPopupPaddingBetweenItems);
    SetLayoutManager(layout);

    icon_ = new ash::FixedSizedImageView(0, ash::kTrayPopupItemHeight);
    AddChildView(icon_);

    AddChildView(percentage_label_);
    AddChildView(separator_label_);
    AddChildView(time_status_label_);
  }
}

void PowerStatusView::UpdateText() {
  const PowerStatus& status = *PowerStatus::Get();
  base::string16 battery_percentage;
  base::string16 battery_time_status;

  if (status.IsBatteryFull()) {
    battery_time_status =
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_BATTERY_FULL);
  } else {
    battery_percentage = base::i18n::MessageFormatter::FormatWithNumberedArgs(
        base::ASCIIToUTF16("{0,number,percent}"),
        static_cast<double>(status.GetRoundedBatteryPercent()) / 100.0);
    if (status.IsUsbChargerConnected()) {
      battery_time_status = l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_BATTERY_CHARGING_UNRELIABLE);
    } else if (status.IsBatteryTimeBeingCalculated()) {
      battery_time_status =
          l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_BATTERY_CALCULATING);
    } else {
      base::TimeDelta time = status.IsBatteryCharging()
                                 ? status.GetBatteryTimeToFull()
                                 : status.GetBatteryTimeToEmpty();
      if (PowerStatus::ShouldDisplayBatteryTime(time) &&
          !status.IsBatteryDischargingOnLinePower()) {
        battery_time_status = l10n_util::GetStringFUTF16(
            status.IsBatteryCharging()
                ? IDS_ASH_STATUS_TRAY_BATTERY_TIME_UNTIL_FULL_SHORT
                : IDS_ASH_STATUS_TRAY_BATTERY_TIME_LEFT_SHORT,
            TimeDurationFormat(time, base::DURATION_WIDTH_NUMERIC));
      }
    }
  }
  percentage_label_->SetVisible(!battery_percentage.empty());
  percentage_label_->SetText(battery_percentage);
  separator_label_->SetVisible(!battery_percentage.empty() &&
                               !battery_time_status.empty());
  time_status_label_->SetVisible(!battery_time_status.empty());
  time_status_label_->SetText(battery_time_status);
}

void PowerStatusView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

gfx::Size PowerStatusView::GetPreferredSize() const {
  gfx::Size size = views::View::GetPreferredSize();
  return gfx::Size(size.width(), kTrayPopupItemHeight);
}

int PowerStatusView::GetHeightForWidth(int width) const {
  return kTrayPopupItemHeight;
}

void PowerStatusView::Layout() {
  views::View::Layout();

  // Move the time_status_label_, separator_label_, and percentage_label_
  // closer to each other.
  if (percentage_label_ && separator_label_ && time_status_label_ &&
      percentage_label_->visible() && time_status_label_->visible()) {
    separator_label_->SetX(percentage_label_->bounds().right() + 1);
    time_status_label_->SetX(separator_label_->bounds().right() + 1);
  }
}

}  // namespace ash
