// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/power_status_view.h"

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/system/power/tray_power.h"
#include "ash/system/tray/fixed_sized_image_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_views.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"

namespace ash {
namespace internal {

namespace {

// Top/bottom padding of the text items.
const int kPaddingVertical = 10;
// Specify min width of status label for layout.
const int kLabelMinWidth = 120;
// Padding between battery status text and battery icon on default view.
const int kPaddingBetweenBatteryStatusAndIcon = 3;
// Minimum battery percentage rendered in UI.
const int kMinBatteryPercent = 1;
}  // namespace

PowerStatusView::PowerStatusView(ViewType view_type,
                                 bool default_view_right_align)
    : default_view_right_align_(default_view_right_align),
      status_label_(NULL),
      time_label_(NULL),
      time_status_label_(NULL),
      percentage_label_(NULL),
      icon_(NULL),
      icon_image_index_(-1),
      view_type_(view_type) {
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
  Update();
}

void PowerStatusView::UpdatePowerStatus(const PowerSupplyStatus& status) {
  supply_status_ = status;
  // Sanitize.
  if (supply_status_.battery_is_full)
    supply_status_.battery_percentage = 100.0;

  Update();
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

    icon_ =
        new ash::internal::FixedSizedImageView(0, ash::kTrayPopupItemHeight);
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

void PowerStatusView::UpdateText() {
  view_type_ == VIEW_DEFAULT ?
      UpdateTextForDefaultView() : UpdateTextForNotificationView();
}

void PowerStatusView::UpdateTextForDefaultView() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  string16 battery_percentage = string16();
  string16 battery_time_status = string16();
  if (supply_status_.line_power_on && supply_status_.battery_is_full) {
    battery_time_status =
        rb.GetLocalizedString(IDS_ASH_STATUS_TRAY_BATTERY_FULL);
    accessible_name_ = rb.GetLocalizedString(
        IDS_ASH_STATUS_TRAY_BATTERY_FULL_CHARGE_ACCESSIBLE);
  } else if (supply_status_.battery_percentage < 0.0f) {
    battery_time_status =
        rb.GetLocalizedString(IDS_ASH_STATUS_TRAY_BATTERY_CALCULATING);
    accessible_name_ = rb.GetLocalizedString(
        IDS_ASH_STATUS_TRAY_BATTERY_CALCULATING_ACCESSIBLE);
  } else {
    battery_percentage = l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_BATTERY_PERCENT_ONLY,
        base::IntToString16(GetRoundedBatteryPercentage()));
    string16 battery_percentage_accessbile = l10n_util::GetStringFUTF16(
        supply_status_.line_power_on ?
            IDS_ASH_STATUS_TRAY_BATTERY_PERCENT_CHARGING_ACCESSIBLE:
            IDS_ASH_STATUS_TRAY_BATTERY_PERCENT_ACCESSIBLE ,
        base::IntToString16(GetRoundedBatteryPercentage()));
    string16 battery_time_accessible = string16();
    int hour = 0;
    int min = 0;
    if (supply_status_.is_calculating_battery_time) {
      battery_time_status =
          rb.GetLocalizedString(IDS_ASH_STATUS_TRAY_BATTERY_CALCULATING);
      battery_time_accessible = rb.GetLocalizedString(
          IDS_ASH_STATUS_TRAY_BATTERY_CALCULATING_ACCESSIBLE);
    } else {
      base::TimeDelta time = base::TimeDelta::FromSeconds(
          supply_status_.line_power_on ?
              supply_status_.averaged_battery_time_to_full :
              supply_status_.averaged_battery_time_to_empty);
      hour = time.InHours();
      min = (time - base::TimeDelta::FromHours(hour)).InMinutes();
      if (hour || min) {
        string16 minute = min < 10 ?
            ASCIIToUTF16("0") + base::IntToString16(min) :
            base::IntToString16(min);
        battery_time_status =
            l10n_util::GetStringFUTF16(
                supply_status_.line_power_on ?
                    IDS_ASH_STATUS_TRAY_BATTERY_TIME_UNTIL_FULL_SHORT :
                    IDS_ASH_STATUS_TRAY_BATTERY_TIME_LEFT_SHORT,
                base::IntToString16(hour),
                minute);
        battery_time_accessible =
            l10n_util::GetStringFUTF16(
                supply_status_.line_power_on ?
                    IDS_ASH_STATUS_TRAY_BATTERY_TIME_UNTIL_FULL_ACCESSIBLE :
                    IDS_ASH_STATUS_TRAY_BATTERY_TIME_LEFT_ACCESSIBLE,
                GetBatteryTimeAccessibilityString(hour, min));
      }
    }
    battery_percentage = battery_time_status.empty() ?
        battery_percentage : battery_percentage + ASCIIToUTF16(" - ");
    accessible_name_ = battery_time_accessible.empty() ?
        battery_percentage_accessbile :
        battery_percentage_accessbile + ASCIIToUTF16(". ")
            + battery_time_accessible;
  }
  percentage_label_->SetVisible(!battery_percentage.empty());
  percentage_label_->SetText(battery_percentage);
  time_status_label_->SetVisible(!battery_time_status.empty());
  time_status_label_->SetText(battery_time_status);
}

void PowerStatusView::UpdateTextForNotificationView() {
  int hour = 0;
  int min = 0;
  if (!supply_status_.is_calculating_battery_time) {
    base::TimeDelta time = base::TimeDelta::FromSeconds(
        supply_status_.line_power_on ?
        supply_status_.averaged_battery_time_to_full :
        supply_status_.averaged_battery_time_to_empty);
    hour = time.InHours();
    min = (time - base::TimeDelta::FromHours(hour)).InMinutes();
  }

  if (supply_status_.line_power_on && supply_status_.battery_is_full) {
    status_label_->SetText(
        ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
            IDS_ASH_STATUS_TRAY_BATTERY_FULL));
  } else {
    if (supply_status_.battery_percentage < 0.0f) {
        status_label_->SetText(
            ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
                IDS_ASH_STATUS_TRAY_BATTERY_CALCULATING));
    } else {
      status_label_->SetText(
          l10n_util::GetStringFUTF16(
              IDS_ASH_STATUS_TRAY_BATTERY_PERCENT,
              base::IntToString16(GetRoundedBatteryPercentage())));
    }
  }

  if (supply_status_.is_calculating_battery_time) {
    time_label_->SetText(
        ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
            IDS_ASH_STATUS_TRAY_BATTERY_CALCULATING));
  } else if (hour || min) {
    if (supply_status_.line_power_on) {
      time_label_->SetText(
          l10n_util::GetStringFUTF16(
              IDS_ASH_STATUS_TRAY_BATTERY_TIME_UNTIL_FULL,
              base::IntToString16(hour),
              base::IntToString16(min)));
    } else {
      // This is a low battery warning, which prompts user when battery
      // time left is not much (ie in minutes).
      min = hour * 60 + min;
      ShellDelegate* delegate = Shell::GetInstance()->delegate();
      if (delegate) {
        time_label_->SetText(delegate->GetTimeRemainingString(
            base::TimeDelta::FromMinutes(min)));
      } else {
        time_label_->SetText(string16());
      }
    }
  } else {
    time_label_->SetText(string16());
  }
}

int PowerStatusView::GetRoundedBatteryPercentage() const {
  DCHECK(supply_status_.battery_percentage >= 0.0f);
  return std::max(kMinBatteryPercent,
      static_cast<int>(supply_status_.battery_percentage));
}

string16 PowerStatusView::GetBatteryTimeAccessibilityString(
    int hour, int min) {
  DCHECK(hour || min);
  if (hour && !min) {
    return Shell::GetInstance()->delegate()->GetTimeDurationLongString(
        base::TimeDelta::FromHours(hour));
  } else if (min  && !hour) {
    return Shell::GetInstance()->delegate()->GetTimeDurationLongString(
        base::TimeDelta::FromMinutes(min));
  } else {
    return l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_BATTERY_TIME_ACCESSIBLE,
        Shell::GetInstance()->delegate()->GetTimeDurationLongString(
            base::TimeDelta::FromHours(hour)),
        Shell::GetInstance()->delegate()->GetTimeDurationLongString(
            base::TimeDelta::FromMinutes(min)));
  }
}

void PowerStatusView::UpdateIcon() {
  if (icon_) {
    int index = TrayPower::GetBatteryImageIndex(supply_status_);
    if (icon_image_index_ != index) {
      icon_image_index_ = index;
      if (icon_image_index_ != -1) {
        icon_->SetImage(
            TrayPower::GetBatteryImage(icon_image_index_, ICON_DARK));
      }
    }
    icon_->SetVisible(true);
  }
}

void PowerStatusView::Update() {
  UpdateText();
  UpdateIcon();
}

void PowerStatusView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

gfx::Size PowerStatusView::GetPreferredSize() {
  gfx::Size size = views::View::GetPreferredSize();
  return gfx::Size(size.width(), kTrayPopupItemHeight);
}

void PowerStatusView::Layout() {
  views::View::Layout();

  // Move the time_status_label_ closer to percentage_label_.
  if (percentage_label_ && time_status_label_ &&
      percentage_label_->visible() && time_status_label_->visible()) {
    time_status_label_->SetX(percentage_label_->bounds().right() + 1);
  }
}

}  // namespace internal
}  // namespace ash
