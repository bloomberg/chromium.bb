// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/power/power_status_view.h"

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/system/chromeos/power/power_status.h"
#include "ash/system/chromeos/power/tray_power.h"
#include "ash/system/tray/fixed_sized_image_view.h"
#include "ash/system/tray/tray_constants.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"

namespace ash {

// Padding between battery status text and battery icon on default view.
const int kPaddingBetweenBatteryStatusAndIcon = 3;

PowerStatusView::PowerStatusView(ViewType view_type,
                                 bool default_view_right_align)
    : default_view_right_align_(default_view_right_align),
      status_label_(NULL),
      time_label_(NULL),
      time_status_label_(NULL),
      percentage_label_(NULL),
      icon_(NULL),
      view_type_(view_type) {
  PowerStatus::Get()->AddObserver(this);
  if (view_type == VIEW_DEFAULT) {
    time_status_label_ = new views::Label;
    percentage_label_ = new views::Label;
    percentage_label_->SetEnabledColor(kHeaderTextColorNormal);
    LayoutDefaultView();
  } else {
    status_label_ = new views::Label;
    time_label_ = new views::Label;
    LayoutNotificationView();
  }
  OnPowerStatusChanged();
}

PowerStatusView::~PowerStatusView() {
  PowerStatus::Get()->RemoveObserver(this);
}

void PowerStatusView::OnPowerStatusChanged() {
  view_type_ == VIEW_DEFAULT ?
      UpdateTextForDefaultView() : UpdateTextForNotificationView();

  if (icon_) {
    icon_->SetImage(
        PowerStatus::Get()->GetBatteryImage(PowerStatus::ICON_DARK));
    icon_->SetVisible(true);
  }
}

void PowerStatusView::LayoutDefaultView() {
  if (default_view_right_align_) {
    views::BoxLayout* layout =
        new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0,
                             kPaddingBetweenBatteryStatusAndIcon);
    SetLayoutManager(layout);

    AddChildView(percentage_label_);
    AddChildView(time_status_label_);

    icon_ = new views::ImageView;
    AddChildView(icon_);
  } else {
    // PowerStatusView is left aligned on the system tray pop up item.
    views::BoxLayout* layout =
        new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0,
                             kTrayPopupPaddingBetweenItems);
    SetLayoutManager(layout);

    icon_ = new ash::FixedSizedImageView(0, ash::kTrayPopupItemHeight);
    AddChildView(icon_);

    AddChildView(percentage_label_);
    AddChildView(time_status_label_);
  }
}

void PowerStatusView::LayoutNotificationView() {
  SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 1));
  status_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(status_label_);

  time_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(time_label_);
}

void PowerStatusView::UpdateTextForDefaultView() {
  const PowerStatus& status = *PowerStatus::Get();
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  base::string16 battery_percentage;
  base::string16 battery_time_status;

  if (status.IsBatteryFull()) {
    battery_time_status =
        rb.GetLocalizedString(IDS_ASH_STATUS_TRAY_BATTERY_FULL);
  } else {
    battery_percentage = l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_BATTERY_PERCENT_ONLY,
        base::IntToString16(status.GetRoundedBatteryPercent()));
    if (status.IsUsbChargerConnected()) {
      battery_time_status = rb.GetLocalizedString(
          IDS_ASH_STATUS_TRAY_BATTERY_CHARGING_UNRELIABLE);
    } else if (status.IsBatteryTimeBeingCalculated()) {
      battery_time_status =
          rb.GetLocalizedString(IDS_ASH_STATUS_TRAY_BATTERY_CALCULATING);
    } else {
      base::TimeDelta time = status.IsBatteryCharging() ?
          status.GetBatteryTimeToFull() : status.GetBatteryTimeToEmpty();
      if (PowerStatus::ShouldDisplayBatteryTime(time) &&
          !status.IsBatteryDischargingOnLinePower()) {
        int hour = 0, min = 0;
        PowerStatus::SplitTimeIntoHoursAndMinutes(time, &hour, &min);
        base::string16 minute = min < 10 ?
            base::ASCIIToUTF16("0") + base::IntToString16(min) :
            base::IntToString16(min);
        battery_time_status =
            l10n_util::GetStringFUTF16(
                status.IsBatteryCharging() ?
                IDS_ASH_STATUS_TRAY_BATTERY_TIME_UNTIL_FULL_SHORT :
                IDS_ASH_STATUS_TRAY_BATTERY_TIME_LEFT_SHORT,
                base::IntToString16(hour),
                minute);
      }
    }
    battery_percentage = battery_time_status.empty() ?
        battery_percentage : battery_percentage + base::ASCIIToUTF16(" - ");
  }
  percentage_label_->SetVisible(!battery_percentage.empty());
  percentage_label_->SetText(battery_percentage);
  time_status_label_->SetVisible(!battery_time_status.empty());
  time_status_label_->SetText(battery_time_status);
}

void PowerStatusView::UpdateTextForNotificationView() {
  const PowerStatus& status = *PowerStatus::Get();
  if (status.IsBatteryFull()) {
    status_label_->SetText(
        ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
            IDS_ASH_STATUS_TRAY_BATTERY_FULL));
  } else {
    status_label_->SetText(
        l10n_util::GetStringFUTF16(
            IDS_ASH_STATUS_TRAY_BATTERY_PERCENT,
            base::IntToString16(status.GetRoundedBatteryPercent())));
  }

  const base::TimeDelta time = status.IsBatteryCharging() ?
      status.GetBatteryTimeToFull() : status.GetBatteryTimeToEmpty();

  if (status.IsUsbChargerConnected()) {
    time_label_->SetText(
        ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
            IDS_ASH_STATUS_TRAY_BATTERY_CHARGING_UNRELIABLE));
  } else if (status.IsBatteryTimeBeingCalculated()) {
    time_label_->SetText(
        ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
            IDS_ASH_STATUS_TRAY_BATTERY_CALCULATING));
  } else if (PowerStatus::ShouldDisplayBatteryTime(time) &&
             !status.IsBatteryDischargingOnLinePower()) {
    int hour = 0, min = 0;
    PowerStatus::SplitTimeIntoHoursAndMinutes(time, &hour, &min);
    if (status.IsBatteryCharging()) {
      time_label_->SetText(
          l10n_util::GetStringFUTF16(
              IDS_ASH_STATUS_TRAY_BATTERY_TIME_UNTIL_FULL,
              base::IntToString16(hour),
              base::IntToString16(min)));
    } else {
      // This is a low battery warning prompting the user in minutes.
      time_label_->SetText(ui::TimeFormat::Simple(
          ui::TimeFormat::FORMAT_REMAINING, ui::TimeFormat::LENGTH_LONG,
          base::TimeDelta::FromMinutes(hour * 60 + min)));
    }
  } else {
    time_label_->SetText(base::string16());
  }
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

  // Move the time_status_label_ closer to percentage_label_.
  if (percentage_label_ && time_status_label_ &&
      percentage_label_->visible() && time_status_label_->visible()) {
    time_status_label_->SetX(percentage_label_->bounds().right() + 1);
  }
}

}  // namespace ash
