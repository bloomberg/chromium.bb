// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/tray_power.h"

#include "ash/ash_switches.h"
#include "ash/shell.h"
#include "ash/system/date/date_view.h"
#include "ash/system/power/power_status_view.h"
#include "ash/system/power/power_supply_status.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_notification_view.h"
#include "ash/system/tray/tray_views.h"
#include "base/command_line.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "third_party/icu/public/i18n/unicode/fieldpos.h"
#include "third_party/icu/public/i18n/unicode/fmtable.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/size.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

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

}  // namespace

namespace tray {

// This view is used only for the tray.
class PowerTrayView : public views::ImageView {
 public:
  PowerTrayView() : battery_icon_index_(-1) {
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
    int index = TrayPower::GetBatteryImageIndex(supply_status_);
    if (battery_icon_index_ != index) {
      battery_icon_index_ = index;
      if (battery_icon_index_ != -1)
        SetImage(TrayPower::GetBatteryImage(battery_icon_index_, ICON_LIGHT));
    }
  }

  PowerSupplyStatus supply_status_;

  // Index of the current icon in the icon array image, or -1 if unknown.
  int battery_icon_index_;

  DISALLOW_COPY_AND_ASSIGN(PowerTrayView);
};

class PowerNotificationView : public TrayNotificationView {
 public:
  explicit PowerNotificationView(TrayPower* owner)
      : TrayNotificationView(owner, 0),
        battery_icon_index_(-1) {
    power_status_view_ =
        new PowerStatusView(PowerStatusView::VIEW_NOTIFICATION, true);
    InitView(power_status_view_);
  }

  void UpdatePowerStatus(const PowerSupplyStatus& status) {
    int index = TrayPower::GetBatteryImageIndex(status);
    if (battery_icon_index_ != index) {
      battery_icon_index_ = index;
      if (battery_icon_index_ != -1) {
        SetIconImage(TrayPower::GetBatteryImage(battery_icon_index_,
                                                ICON_DARK));
      }
    }
    power_status_view_->UpdatePowerStatus(status);
  }

 private:
  PowerStatusView* power_status_view_;

  // Index of the current icon in the icon array image, or -1 if unknown.
  int battery_icon_index_;

  DISALLOW_COPY_AND_ASSIGN(PowerNotificationView);
};

}  // namespace tray

using tray::PowerNotificationView;

TrayPower::TrayPower(SystemTray* system_tray)
    : SystemTrayItem(system_tray),
      power_tray_(NULL),
      notification_view_(NULL),
      notification_state_(NOTIFICATION_NONE) {
  Shell::GetInstance()->system_tray_notifier()->AddPowerStatusObserver(this);
}

TrayPower::~TrayPower() {
  Shell::GetInstance()->system_tray_notifier()->RemovePowerStatusObserver(this);
}

// static
int TrayPower::GetBatteryImageIndex(const PowerSupplyStatus& supply_status) {
  int image_index = 0;
  if (supply_status.battery_percentage >= 100) {
    image_index = kNumPowerImages - 1;
  } else if (!supply_status.battery_is_present) {
    image_index = kNumPowerImages;
  } else {
    image_index = static_cast<int>(supply_status.battery_percentage /
                  100.0 * (kNumPowerImages - 1));
    image_index = std::max(std::min(image_index, kNumPowerImages - 2), 0);
  }
  return (image_index << 1) | (supply_status.line_power_on ? 1 : 0);
}

// static
gfx::ImageSkia TrayPower::GetBatteryImage(int image_index, IconSet icon_set) {
  gfx::Image all = ui::ResourceBundle::GetSharedInstance().GetImageNamed(
      icon_set == ICON_DARK ?
      IDR_AURA_UBER_TRAY_POWER_SMALL_DARK : IDR_AURA_UBER_TRAY_POWER_SMALL);

  gfx::Rect region(
      (image_index & 0x1) ? kBatteryImageWidth : 0,
      (image_index >> 1) * kBatteryImageHeight,
      kBatteryImageWidth, kBatteryImageHeight);
  return gfx::ImageSkiaOperations::ExtractSubset(*all.ToImageSkia(), region);
}

views::View* TrayPower::CreateTrayView(user::LoginStatus status) {
  // There may not be enough information when this is created about whether
  // there is a battery or not. So always create this, and adjust visibility as
  // necessary.
  PowerSupplyStatus power_status =
      ash::Shell::GetInstance()->system_tray_delegate()->GetPowerSupplyStatus();
  CHECK(power_tray_ == NULL);
  power_tray_ = new tray::PowerTrayView();
  power_tray_->UpdatePowerStatus(power_status);
  return power_tray_;
}

views::View* TrayPower::CreateDefaultView(user::LoginStatus status) {
  // Make sure icon status is up-to-date. (Also triggers stub activation).
  ash::Shell::GetInstance()->system_tray_delegate()->RequestStatusUpdate();
  return NULL;
}

views::View* TrayPower::CreateNotificationView(user::LoginStatus status) {
  CHECK(notification_view_ == NULL);
  PowerSupplyStatus power_status =
      ash::Shell::GetInstance()->system_tray_delegate()->GetPowerSupplyStatus();
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

void TrayPower::UpdateAfterShelfAlignmentChange(ShelfAlignment alignment) {
  SetTrayImageItemBorder(power_tray_, alignment);
}

void TrayPower::OnPowerStatusChanged(const PowerSupplyStatus& status) {
  if (power_tray_)
    power_tray_->UpdatePowerStatus(status);
  if (notification_view_)
    notification_view_->UpdatePowerStatus(status);

  if (UpdateNotificationState(status))
    ShowNotificationView();
  else if (notification_state_ == NOTIFICATION_NONE)
    HideNotificationView();
}

bool TrayPower::UpdateNotificationState(const PowerSupplyStatus& status) {
  if (!status.battery_is_present ||
      status.is_calculating_battery_time ||
      status.line_power_on) {
    notification_state_ = NOTIFICATION_NONE;
    return false;
  }

  int remaining_seconds = status.averaged_battery_time_to_empty;
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
