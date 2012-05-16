// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/tray_power.h"

#include "ash/ash_switches.h"
#include "ash/shell.h"
#include "ash/system/date/date_view.h"
#include "ash/system/power/power_supply_status.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_views.h"
#include "base/command_line.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "grit/ash_strings.h"
#include "grit/ui_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/size.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "unicode/fieldpos.h"
#include "unicode/fmtable.h"

namespace ash {
namespace internal {

namespace {
// Width and height of battery images.
const int kBatteryImageHeight = 25;
const int kBatteryImageWidth = 25;
// Number of different power states.
const int kNumPowerImages = 15;
// Top/bottom padding of the text items.
const int kPaddingVertical = 10;
// Specify min width of status label for layout.
const int kLabelMinWidth = 120;
// Notification times.
const int kCriticalSeconds = 5 * 60;
const int kLowPowerSeconds = 15 * 60;
const int kNoWarningSeconds = 30 * 60;

enum IconSet {
  ICON_LIGHT,
  ICON_DARK
};

SkBitmap GetBatteryImage(const PowerSupplyStatus& supply_status,
                         IconSet icon_set) {
  SkBitmap image;
  gfx::Image all = ui::ResourceBundle::GetSharedInstance().GetImageNamed(
      icon_set == ICON_DARK ?
      IDR_AURA_UBER_TRAY_POWER_SMALL_DARK : IDR_AURA_UBER_TRAY_POWER_SMALL);

  int image_index = 0;
  if (supply_status.battery_percentage >= 100) {
    image_index = kNumPowerImages - 1;
  } else if (!supply_status.battery_is_present) {
    image_index = kNumPowerImages;
  } else {
    double percentage = supply_status.is_calculating_battery_time ? 100.0 :
        supply_status.battery_percentage;
    image_index = static_cast<int>(percentage / 100.0 * (kNumPowerImages - 1));
    image_index = std::max(std::min(image_index, kNumPowerImages - 2), 0);
  }

  // TODO(mbolohan): Remove the 2px offset when the assets are centered. See
  // crbug.com/119832.
  SkIRect region = SkIRect::MakeXYWH(
      (supply_status.line_power_on ? kBatteryImageWidth : 0) + 2,
      image_index * kBatteryImageHeight,
      kBatteryImageWidth - 2, kBatteryImageHeight);
  all.ToSkBitmap()->extractSubset(&image, region);
  return image;
}

}  // namespace

namespace tray {

// This view is used only for the tray.
class PowerTrayView : public views::ImageView {
 public:
  PowerTrayView() {
    UpdateImage();
  }

  virtual ~PowerTrayView() {
  }

  void UpdatePowerStatus(const PowerSupplyStatus& status) {
    supply_status_ = status;
    // Sanitize.
    if (supply_status_.battery_is_full)
      supply_status_.battery_percentage = 100.0;

    UpdateImage();
    SetVisible(status.battery_is_present);
  }

 private:
  void UpdateImage() {
    SetImage(GetBatteryImage(supply_status_, ICON_LIGHT));
  }

  PowerSupplyStatus supply_status_;

  DISALLOW_COPY_AND_ASSIGN(PowerTrayView);
};

// This view is used only for the popup.
class PowerStatusView : public views::View {
 public:
  enum ViewType {
    VIEW_DEFAULT,
    VIEW_NOTIFICATION
  };

  explicit PowerStatusView(ViewType view_type) {
    status_label_ = new views::Label;
    status_label_->SetHorizontalAlignment(views::Label::ALIGN_RIGHT);
    time_label_ = new views::Label;
    time_label_->SetHorizontalAlignment(views::Label::ALIGN_RIGHT);

    icon_ = new views::ImageView;

    AddChildView(status_label_);
    AddChildView(time_label_);
    AddChildView(icon_);

    if (view_type == VIEW_DEFAULT)
      LayoutDefaultView();
    else
      LayoutNotificationView();

    Update();

    set_border(views::Border::CreateEmptyBorder(
        kPaddingVertical, kTrayPopupPaddingHorizontal,
        kPaddingVertical, kTrayPopupPaddingHorizontal));
  }

  virtual ~PowerStatusView() {
  }

  void UpdatePowerStatus(const PowerSupplyStatus& status) {
    supply_status_ = status;
    // Sanitize.
    if (supply_status_.battery_is_full)
      supply_status_.battery_percentage = 100.0;

    Update();
  }

 private:
  void LayoutDefaultView() {
    views::GridLayout* layout = new views::GridLayout(this);
    SetLayoutManager(layout);

    views::ColumnSet* columns = layout->AddColumnSet(0);

    columns->AddPaddingColumn(0, kTrayPopupPaddingHorizontal);
    // Status + Time
    columns->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                       0, views::GridLayout::USE_PREF, 0, kLabelMinWidth);

    columns->AddPaddingColumn(0, kTrayPopupPaddingHorizontal/2);

    // Icon
    columns->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                       0, views::GridLayout::USE_PREF, 0, 0);

    columns->AddPaddingColumn(0, kTrayPopupPaddingHorizontal);

    layout->AddPaddingRow(0, kPaddingVertical);

    layout->StartRow(0, 0);
    layout->AddView(status_label_);
    layout->AddView(icon_, 1, 3);  // 3 rows for icon

    layout->AddPaddingRow(0, kPaddingVertical/3);
    layout->StartRow(0, 0);
    layout->AddView(time_label_);

    layout->AddPaddingRow(0, kPaddingVertical);
  }

  void LayoutNotificationView() {
    views::GridLayout* layout = new views::GridLayout(this);
    SetLayoutManager(layout);

    views::ColumnSet* columns = layout->AddColumnSet(0);

    // Icon
    columns->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                       0, views::GridLayout::USE_PREF, 0, 0);

    columns->AddPaddingColumn(0, kTrayPopupPaddingHorizontal/2);

    // Status + Time
    columns->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                       0, views::GridLayout::USE_PREF, 0, kLabelMinWidth);

    layout->AddPaddingRow(0, kPaddingVertical);

    layout->StartRow(0, 0);
    layout->AddView(icon_, 1, 3);  // 3 rows for icon
    layout->AddView(status_label_);
    layout->AddPaddingRow(0, kPaddingVertical/3);

    layout->StartRow(0, 0);
    layout->SkipColumns(1);
    layout->AddView(time_label_);

    layout->AddPaddingRow(0, kPaddingVertical);
  }

  void UpdateText() {
    base::TimeDelta time = base::TimeDelta::FromSeconds(
        supply_status_.line_power_on ?
        supply_status_.averaged_battery_time_to_full :
        supply_status_.averaged_battery_time_to_empty);
    int hour = time.InHours();
    int min = (time - base::TimeDelta::FromHours(hour)).InMinutes();

    if (supply_status_.line_power_on && !hour && !min) {
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

  void UpdateIcon() {
    icon_->SetImage(GetBatteryImage(supply_status_, ICON_DARK));
    icon_->SetVisible(true);
  }

  void Update() {
    UpdateText();
    UpdateIcon();
  }

  views::Label* status_label_;
  views::Label* time_label_;
  views::ImageView* icon_;

  PowerSupplyStatus supply_status_;

  DISALLOW_COPY_AND_ASSIGN(PowerStatusView);
};

class PowerNotificationView : public TrayNotificationView {
 public:
  explicit PowerNotificationView(TrayPower* tray)
      : tray_(tray) {
    power_status_view_ =
        new PowerStatusView(PowerStatusView::VIEW_NOTIFICATION);
    InitView(power_status_view_);
  }

  void UpdatePowerStatus(const PowerSupplyStatus& status) {
    power_status_view_->UpdatePowerStatus(status);
  }

  // Overridden from TrayNotificationView:
  virtual void OnClose() OVERRIDE {
    tray_->HideNotificationView();
  }

 private:
  TrayPower* tray_;
  tray::PowerStatusView* power_status_view_;

  DISALLOW_COPY_AND_ASSIGN(PowerNotificationView);
};

}  // namespace tray

using tray::PowerStatusView;
using tray::PowerNotificationView;

TrayPower::TrayPower()
    : power_tray_(NULL),
      notification_view_(NULL),
      notification_state_(NOTIFICATION_NONE) {
}

TrayPower::~TrayPower() {
}

views::View* TrayPower::CreateTrayView(user::LoginStatus status) {
  // There may not be enough information when this is created about whether
  // there is a battery or not. So always create this, and adjust visibility as
  // necessary.
  PowerSupplyStatus power_status =
      ash::Shell::GetInstance()->tray_delegate()->GetPowerSupplyStatus();
  CHECK(power_tray_ == NULL);
  power_tray_ = new tray::PowerTrayView();
  power_tray_->UpdatePowerStatus(power_status);
  return power_tray_;
}

views::View* TrayPower::CreateDefaultView(user::LoginStatus status) {
  // Make sure icon status is up-to-date. (Also triggers stub activation).
  ash::Shell::GetInstance()->tray_delegate()->RequestStatusUpdate();
  return NULL;
}

views::View* TrayPower::CreateNotificationView(user::LoginStatus status) {
  CHECK(notification_view_ == NULL);
  PowerSupplyStatus power_status =
      ash::Shell::GetInstance()->tray_delegate()->GetPowerSupplyStatus();
  if (!power_status.battery_is_present)
    return NULL;

  notification_view_ = new PowerNotificationView(this);
  notification_view_->UpdatePowerStatus(power_status);

  return notification_view_;
}

void TrayPower::DestroyTrayView() {
  power_tray_ = NULL;
}

void TrayPower::DestroyDefaultView() {
}

void TrayPower::DestroyNotificationView() {
  notification_view_ = NULL;
}

void TrayPower::UpdateAfterLoginStatusChange(user::LoginStatus status) {
}

void TrayPower::OnPowerStatusChanged(const PowerSupplyStatus& status) {
  if (power_tray_)
    power_tray_->UpdatePowerStatus(status);
  if (notification_view_)
    notification_view_->UpdatePowerStatus(status);

  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAshNotify)) {
    if (UpdateNotificationState(status))
      ShowNotificationView();
    else if (notification_state_ == NOTIFICATION_NONE)
      HideNotificationView();
  }
}

bool TrayPower::UpdateNotificationState(const PowerSupplyStatus& status) {
  if (!status.battery_is_present ||
      status.is_calculating_battery_time ||
      status.line_power_on) {
    notification_state_ = NOTIFICATION_NONE;
    return false;
  }

  int remaining_seconds = status.battery_seconds_to_empty;
  if (remaining_seconds >= kNoWarningSeconds) {
    notification_state_ = NOTIFICATION_NONE;
    return false;
  }

  switch (notification_state_) {
    case NOTIFICATION_NONE:
      if (remaining_seconds <= kCriticalSeconds) {
        notification_state_ = NOTIFICATION_CRITICAL;
        return true;
      } else if (remaining_seconds <= kLowPowerSeconds) {
        notification_state_ = NOTIFICATION_LOW_POWER;
        return true;
      }
      return false;
    case NOTIFICATION_LOW_POWER:
      if (remaining_seconds <= kCriticalSeconds) {
        notification_state_ = NOTIFICATION_CRITICAL;
        return true;
      }
      return false;
    case NOTIFICATION_CRITICAL:
      return false;
  }
  NOTREACHED();
  return false;
}

}  // namespace internal
}  // namespace ash
