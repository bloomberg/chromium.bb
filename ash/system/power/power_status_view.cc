// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/power_status_view.h"

#include "ash/strings/grit/ash_strings.h"
#include "ash/system/power/tray_power.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_item_style.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "base/i18n/number_formatting.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"

namespace ash {

PowerStatusView::PowerStatusView()
    : percentage_label_(new views::Label),
      separator_label_(new views::Label),
      time_status_label_(new views::Label) {
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);

  percentage_label_->SetEnabledColor(kHeaderTextColorNormal);
  separator_label_->SetEnabledColor(kHeaderTextColorNormal);
  separator_label_->SetText(base::ASCIIToUTF16(" - "));

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kHorizontal, gfx::Insets(0, 12),
      kTrayPopupPaddingBetweenItems));

  AddChildView(percentage_label_);
  AddChildView(separator_label_);
  AddChildView(time_status_label_);

  PowerStatus::Get()->AddObserver(this);
  OnPowerStatusChanged();
}

PowerStatusView::~PowerStatusView() {
  PowerStatus::Get()->RemoveObserver(this);
}

void PowerStatusView::OnPowerStatusChanged() {
  UpdateText();
}

void PowerStatusView::UpdateText() {
  const PowerStatus& status = *PowerStatus::Get();
  base::string16 battery_percentage;
  base::string16 battery_time_status;

  if (status.IsBatteryFull()) {
    battery_time_status =
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_BATTERY_FULL);
  } else {
    battery_percentage = base::FormatPercent(status.GetRoundedBatteryPercent());
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
        base::string16 duration;
        if (!base::TimeDurationFormat(time, base::DURATION_WIDTH_NUMERIC,
                                      &duration))
          LOG(ERROR) << "Failed to format duration " << time;
        battery_time_status = l10n_util::GetStringFUTF16(
            status.IsBatteryCharging()
                ? IDS_ASH_STATUS_TRAY_BATTERY_TIME_UNTIL_FULL_SHORT
                : IDS_ASH_STATUS_TRAY_BATTERY_TIME_LEFT_SHORT,
            duration);
      }
    }
  }
  percentage_label_->SetVisible(!battery_percentage.empty());
  percentage_label_->SetText(battery_percentage);
  separator_label_->SetVisible(!battery_percentage.empty() &&
                               !battery_time_status.empty());
  time_status_label_->SetVisible(!battery_time_status.empty());
  time_status_label_->SetText(battery_time_status);

  accessible_name_ = PowerStatus::Get()->GetAccessibleNameString(true);

  TrayPopupItemStyle style(TrayPopupItemStyle::FontStyle::SYSTEM_INFO);
  style.SetupLabel(percentage_label_);
  style.SetupLabel(separator_label_);
  style.SetupLabel(time_status_label_);
}

void PowerStatusView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
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

void PowerStatusView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kLabelText;
  node_data->SetName(accessible_name_);
}

}  // namespace ash
