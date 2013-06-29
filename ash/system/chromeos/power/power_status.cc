// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/power/power_status.h"

#include <algorithm>
#include <cmath>

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/rect.h"

namespace ash {
namespace internal {

namespace {

base::string16 GetBatteryTimeAccessibilityString(int hour, int min) {
  DCHECK(hour || min);
  if (hour && !min) {
    return Shell::GetInstance()->delegate()->GetTimeDurationLongString(
        base::TimeDelta::FromHours(hour));
  }
  if (min && !hour) {
    return Shell::GetInstance()->delegate()->GetTimeDurationLongString(
        base::TimeDelta::FromMinutes(min));
  }
  return l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_BATTERY_TIME_ACCESSIBLE,
      Shell::GetInstance()->delegate()->GetTimeDurationLongString(
          base::TimeDelta::FromHours(hour)),
      Shell::GetInstance()->delegate()->GetTimeDurationLongString(
          base::TimeDelta::FromMinutes(min)));
}

static PowerStatus* g_power_status = NULL;

// Minimum battery percentage rendered in UI.
const int kMinBatteryPercent = 1;

// Width and height of battery images.
const int kBatteryImageHeight = 25;
const int kBatteryImageWidth = 25;

// Number of different power states.
const int kNumPowerImages = 15;

}  // namespace

// static
void PowerStatus::Initialize() {
  CHECK(!g_power_status);
  g_power_status = new PowerStatus();
}

// static
void PowerStatus::Shutdown() {
  CHECK(g_power_status);
  delete g_power_status;
  g_power_status = NULL;
}

// static
bool PowerStatus::IsInitialized() {
  return g_power_status != NULL;
}

// static
PowerStatus* PowerStatus::Get() {
  CHECK(g_power_status) << "PowerStatus::Get() called before Initialize().";
  return g_power_status;
}

void PowerStatus::AddObserver(Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void PowerStatus::RemoveObserver(Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

void PowerStatus::RequestStatusUpdate() {
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
      RequestStatusUpdate();
}

bool PowerStatus::IsBatteryPresent() const {
  return status_.battery_is_present;
}

bool PowerStatus::IsBatteryFull() const {
  return status_.battery_is_full;
}

double PowerStatus::GetBatteryPercent() const {
  return status_.battery_percentage;
}

int PowerStatus::GetRoundedBatteryPercent() const {
  return std::max(kMinBatteryPercent,
      static_cast<int>(status_.battery_percentage + 0.5));
}

bool PowerStatus::IsBatteryTimeBeingCalculated() const {
  return status_.is_calculating_battery_time;
}

base::TimeDelta PowerStatus::GetBatteryTimeToEmpty() const {
  return base::TimeDelta::FromSeconds(status_.battery_seconds_to_empty);
}

base::TimeDelta PowerStatus::GetBatteryTimeToFull() const {
  return base::TimeDelta::FromSeconds(status_.battery_seconds_to_full);
}

bool PowerStatus::IsLinePowerConnected() const {
  return status_.line_power_on;
}

bool PowerStatus::IsMainsChargerConnected() const {
  return status_.battery_state == chromeos::PowerSupplyStatus::CHARGING;
}

bool PowerStatus::IsUsbChargerConnected() const {
  return status_.battery_state == chromeos::PowerSupplyStatus::CONNECTED_TO_USB;
}

gfx::ImageSkia PowerStatus::GetBatteryImage(IconSet icon_set) const {
  gfx::Image all;
  if (IsUsbChargerConnected()) {
    all = ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        icon_set == ICON_DARK ?
        IDR_AURA_UBER_TRAY_POWER_SMALL_CHARGING_UNRELIABLE_DARK :
        IDR_AURA_UBER_TRAY_POWER_SMALL_CHARGING_UNRELIABLE);
  } else {
    all = ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        icon_set == ICON_DARK ?
        IDR_AURA_UBER_TRAY_POWER_SMALL_DARK : IDR_AURA_UBER_TRAY_POWER_SMALL);
  }

  // Get the horizontal offset in the battery icon array image.
  int offset = (IsUsbChargerConnected() || !status_.line_power_on) ? 0 : 1;

  // Get the icon index in the battery icon array image.
  int index = -1;
  if (status_.battery_percentage >= 100) {
    index = kNumPowerImages - 1;
  } else if (!status_.battery_is_present) {
    index = kNumPowerImages;
  } else {
    index = static_cast<int>(
        status_.battery_percentage / 100.0 * (kNumPowerImages - 1));
    index = std::max(std::min(index, kNumPowerImages - 2), 0);
  }

  gfx::Rect region(
      offset * kBatteryImageWidth, index * kBatteryImageHeight,
      kBatteryImageWidth, kBatteryImageHeight);
  return gfx::ImageSkiaOperations::ExtractSubset(*all.ToImageSkia(), region);
}

base::string16 PowerStatus::GetAccessibleNameString() const {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  if (status_.line_power_on && status_.battery_is_full) {
    return rb.GetLocalizedString(
        IDS_ASH_STATUS_TRAY_BATTERY_FULL_CHARGE_ACCESSIBLE);
  }
  base::string16 battery_percentage_accessible = l10n_util::GetStringFUTF16(
      IsLinePowerConnected() ?
      IDS_ASH_STATUS_TRAY_BATTERY_PERCENT_CHARGING_ACCESSIBLE :
      IDS_ASH_STATUS_TRAY_BATTERY_PERCENT_ACCESSIBLE,
      base::IntToString16(GetRoundedBatteryPercent()));
  base::string16 battery_time_accessible = base::string16();
  if (IsUsbChargerConnected()) {
    battery_time_accessible = rb.GetLocalizedString(
        IDS_ASH_STATUS_TRAY_BATTERY_CHARGING_UNRELIABLE_ACCESSIBLE);
  } else {
    if (IsBatteryTimeBeingCalculated()) {
      battery_time_accessible = rb.GetLocalizedString(
          IDS_ASH_STATUS_TRAY_BATTERY_CALCULATING_ACCESSIBLE);
    } else {
      base::TimeDelta time = IsLinePowerConnected() ? GetBatteryTimeToFull() :
          GetBatteryTimeToEmpty();
      int hour = time.InHours();
      int min = (time - base::TimeDelta::FromHours(hour)).InMinutes();
      if (hour || min) {
        base::string16 minute = min < 10 ?
            ASCIIToUTF16("0") + base::IntToString16(min) :
            base::IntToString16(min);
        battery_time_accessible =
            l10n_util::GetStringFUTF16(
                IsLinePowerConnected() ?
                IDS_ASH_STATUS_TRAY_BATTERY_TIME_UNTIL_FULL_ACCESSIBLE :
                IDS_ASH_STATUS_TRAY_BATTERY_TIME_LEFT_ACCESSIBLE,
                GetBatteryTimeAccessibilityString(hour, min));
      }
    }
  }
  return battery_time_accessible.empty() ?
      battery_percentage_accessible :
      battery_percentage_accessible + ASCIIToUTF16(". ") +
      battery_time_accessible;
}

PowerStatus::PowerStatus() {
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
      AddObserver(this);
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
      RequestStatusUpdate();
}

PowerStatus::~PowerStatus() {
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
      RemoveObserver(this);
}

void PowerStatus::PowerChanged(const chromeos::PowerSupplyStatus& status) {
  status_ = status;
  if (status_.battery_is_full)
    status_.battery_percentage = 100.0;

  FOR_EACH_OBSERVER(Observer, observers_, OnPowerStatusChanged());
}

}  // namespace internal
}  // namespace ash
