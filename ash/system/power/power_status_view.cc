// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/power_status_view.h"

#include "ash/system/power/tray_power.h"
#include "ash/system/tray/tray_constants.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "grit/ash_strings.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/l10n/l10n_util.h"
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
}  // namespace

PowerStatusView::PowerStatusView(ViewType view_type)
    : status_label_(NULL),
      time_label_(NULL),
      time_status_label_(NULL),
      icon_(NULL),
      view_type_(view_type) {
  if (view_type == VIEW_DEFAULT) {
    time_status_label_ = new views::Label;
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
  views::BoxLayout* layout =
      new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0,
                           kPaddingBetweenBatteryStatusAndIcon);
  SetLayoutManager(layout);

  time_status_label_->SetHorizontalAlignment(views::Label::ALIGN_RIGHT);
  AddChildView(time_status_label_);

  icon_ = new views::ImageView;
  AddChildView(icon_);
}

void PowerStatusView::LayoutNotificationView() {
  SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 1));
  status_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  AddChildView(status_label_);

  time_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  AddChildView(time_label_);
}

void PowerStatusView::UpdateText() {
  view_type_ == VIEW_DEFAULT ?
      UpdateTextForDefaultView() : UpdateTextForNotificationView();
}

void PowerStatusView::UpdateTextForDefaultView() {
  int hour = 0;
  int min = 0;
  if (!supply_status_.is_calculating_battery_time) {
    base::TimeDelta time = base::TimeDelta::FromSeconds(
        supply_status_.averaged_battery_time_to_empty);
    hour = time.InHours();
    min = (time - base::TimeDelta::FromHours(hour)).InMinutes();
  }

  if (supply_status_.line_power_on && supply_status_.battery_is_full) {
    time_status_label_->SetText(
        ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
            IDS_ASH_STATUS_TRAY_BATTERY_FULL));
  } else {
    string16 battery_percentage = l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_BATTERY_PERCENT_ONLY,
        base::IntToString16(
            static_cast<int>(supply_status_.battery_percentage)));
    string16 battery_time = string16();
    if (supply_status_.is_calculating_battery_time) {
      battery_time =
          ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
              IDS_ASH_STATUS_TRAY_BATTERY_CALCULATING);
    } else if (hour || min){
      battery_time =
          l10n_util::GetStringFUTF16(
              IDS_ASH_STATUS_TRAY_BATTERY_TIME_ONLY,
              base::IntToString16(hour),
              base::IntToString16(min)) +
          ASCIIToUTF16(" - ");
    }
    string16 battery_status = battery_time + battery_percentage;
    time_status_label_->SetText(battery_status);
  }
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
    status_label_->SetText(
        l10n_util::GetStringFUTF16(
            IDS_ASH_STATUS_TRAY_BATTERY_PERCENT,
            base::IntToString16(
                static_cast<int>(supply_status_.battery_percentage))));
  }

  if (supply_status_.is_calculating_battery_time) {
    time_label_->SetText(
        ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
            IDS_ASH_STATUS_TRAY_BATTERY_CALCULATING));
  } else if (hour || min) {
    time_label_->SetText(
        l10n_util::GetStringFUTF16(
            supply_status_.line_power_on ?
            IDS_ASH_STATUS_TRAY_BATTERY_TIME_UNTIL_FULL :
            IDS_ASH_STATUS_TRAY_BATTERY_TIME_UNTIL_EMPTY,
            base::IntToString16(hour),
            base::IntToString16(min)));
  } else {
    time_label_->SetText(string16());
  }

}

void PowerStatusView::UpdateIcon() {
  if (icon_) {
    icon_->SetImage(TrayPower::GetBatteryImage(supply_status_, ICON_DARK));
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

}  // namespace internal
}  // namespace ash
