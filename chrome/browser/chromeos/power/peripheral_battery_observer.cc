// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/peripheral_battery_observer.h"

#include <vector>

#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/system_notifier.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/theme_resources.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "content/public/browser/browser_thread.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/devices/input_device_manager.h"
#include "ui/events/devices/touchscreen_device.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"

namespace chromeos {

namespace {

// When a peripheral device's battery level is <= kLowBatteryLevel, consider
// it to be in low battery condition.
const int kLowBatteryLevel = 15;

// Don't show 2 low battery notification within |kNotificationInterval|.
constexpr base::TimeDelta kNotificationInterval =
    base::TimeDelta::FromSeconds(60);

// TODO(sammiequon): Add a notification url to chrome://settings/stylus once
// battery related information is shown there.
const char kNotificationOriginUrl[] = "chrome://peripheral-battery";
const char kNotifierId[] = "power.peripheral-battery";

// HID Bluetooth device's battery sysfs entry path looks like
// "/sys/class/power_supply/hid-AA:BB:CC:DD:EE:FF-battery".
// Here the bluetooth address is showed in reverse order and its true
// address "FF:EE:DD:CC:BB:AA".
const char kHIDBatteryPathPrefix[] = "/sys/class/power_supply/hid-";
const char kHIDBatteryPathSuffix[] = "-battery";

bool IsBluetoothHIDBattery(const std::string& path) {
  return base::StartsWith(path, kHIDBatteryPathPrefix,
                          base::CompareCase::INSENSITIVE_ASCII) &&
         base::EndsWith(path, kHIDBatteryPathSuffix,
                        base::CompareCase::INSENSITIVE_ASCII);
}

std::string ExtractBluetoothAddress(const std::string& path) {
  int header_size = strlen(kHIDBatteryPathPrefix);
  int end_size = strlen(kHIDBatteryPathSuffix);
  int key_len = path.size() - header_size - end_size;
  if (key_len <= 0)
    return std::string();
  std::string reverse_address =
      base::ToLowerASCII(path.substr(header_size, key_len));
  std::vector<base::StringPiece> result = base::SplitStringPiece(
      reverse_address, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  std::reverse(result.begin(), result.end());
  std::string address = base::JoinString(result, ":");
  return address;
}

// Checks if the device is an external stylus.
bool IsStylusDevice(const std::string& bluetooth_address,
                    const std::string& model_name) {
  for (const ui::TouchscreenDevice& device :
       ui::InputDeviceManager::GetInstance()->GetTouchscreenDevices()) {
    if (device.is_stylus && device.name == model_name &&
        ExtractBluetoothAddress(device.sys_path.value()) == bluetooth_address) {
      return true;
    }
  }

  return false;
}

// Struct containing parameters for the notification which vary between the
// stylus notifications and the non stylus notifications.
struct NotificationParams {
  std::string id;
  base::string16 title;
  base::string16 message;
  int image_id;
  std::string notifier_name;
  GURL url;
};

NotificationParams GetNonStylusNotificationParams(const std::string& address,
                                                  const std::string& name,
                                                  int battery_level) {
  return NotificationParams{
      address,
      base::ASCIIToUTF16(name),
      l10n_util::GetStringFUTF16Int(
          IDS_ASH_LOW_PERIPHERAL_BATTERY_NOTIFICATION_TEXT, battery_level),
      IDR_NOTIFICATION_PERIPHERAL_BATTERY_LOW,
      kNotifierId,
      GURL(kNotificationOriginUrl)};
}

NotificationParams GetStylusNotificationParams() {
  return NotificationParams{
      PeripheralBatteryObserver::kStylusNotificationId,
      l10n_util::GetStringUTF16(IDS_ASH_LOW_STYLUS_BATTERY_NOTIFICATION_TITLE),
      l10n_util::GetStringUTF16(IDS_ASH_LOW_STYLUS_BATTERY_NOTIFICATION_BODY),
      IDR_NOTIFICATION_STYLUS_BATTERY_LOW,
      ash::system_notifier::kNotifierStylusBattery,
      GURL()};
}

}  // namespace

const char PeripheralBatteryObserver::kStylusNotificationId[] =
    "stylus-battery";

PeripheralBatteryObserver::PeripheralBatteryObserver()
    : testing_clock_(NULL),
      weakptr_factory_(
          new base::WeakPtrFactory<PeripheralBatteryObserver>(this)) {
  DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(this);
  device::BluetoothAdapterFactory::GetAdapter(
      base::Bind(&PeripheralBatteryObserver::InitializeOnBluetoothReady,
                 weakptr_factory_->GetWeakPtr()));
}

PeripheralBatteryObserver::~PeripheralBatteryObserver() {
  if (bluetooth_adapter_.get())
    bluetooth_adapter_->RemoveObserver(this);
  DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(this);
}

void PeripheralBatteryObserver::PeripheralBatteryStatusReceived(
    const std::string& path,
    const std::string& name,
    int level) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::string address;
  if (IsBluetoothHIDBattery(path)) {
    // For HID bluetooth device, device address is used as key to index
    // BatteryInfo.
    address = ExtractBluetoothAddress(path);
  } else {
    LOG(ERROR) << "Unsupported battery path " << path;
    return;
  }

  if (address.empty()) {
    LOG(ERROR) << "No valid battery address at path " << path;
    return;
  }

  if (level < -1 || level > 100) {
    LOG(ERROR) << "Invalid battery level " << level
               << " for device " << name << " at path " << path;
    return;
  }
  // If unknown battery level received, cancel any existing notification.
  if (level == -1) {
    CancelNotification(address);
    return;
  }

  // Post the notification in 2 cases:
  // 1. It's the first time the battery level is received, and it is
  //    below kLowBatteryLevel.
  // 2. The battery level is in record and it drops below kLowBatteryLevel.
  if (batteries_.find(address) == batteries_.end()) {
    BatteryInfo battery{name, level, base::TimeTicks(),
                        IsStylusDevice(address, name)};
    if (level <= kLowBatteryLevel) {
      if (PostNotification(address, battery)) {
        battery.last_notification_timestamp = testing_clock_ ?
            testing_clock_->NowTicks() : base::TimeTicks::Now();
      }
    }
    batteries_[address] = battery;
  } else {
    BatteryInfo* battery = &batteries_[address];
    battery->name = name;
    int old_level = battery->level;
    battery->level = level;
    if (old_level > kLowBatteryLevel && level <= kLowBatteryLevel) {
      if (PostNotification(address, *battery)) {
        battery->last_notification_timestamp = testing_clock_ ?
            testing_clock_->NowTicks() : base::TimeTicks::Now();
      }
    }
  }
}

void PeripheralBatteryObserver::DeviceChanged(device::BluetoothAdapter* adapter,
                                              device::BluetoothDevice* device) {
  if (!device->IsPaired())
    RemoveBattery(device->GetAddress());
}

void PeripheralBatteryObserver::DeviceRemoved(device::BluetoothAdapter* adapter,
                                              device::BluetoothDevice* device) {
  RemoveBattery(device->GetAddress());
}

void PeripheralBatteryObserver::InitializeOnBluetoothReady(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  bluetooth_adapter_ = adapter;
  CHECK(bluetooth_adapter_.get());
  bluetooth_adapter_->AddObserver(this);
}

void PeripheralBatteryObserver::RemoveBattery(const std::string& address) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::string address_lowercase = base::ToLowerASCII(address);
  std::map<std::string, BatteryInfo>::iterator it =
      batteries_.find(address_lowercase);
  if (it != batteries_.end()) {
    CancelNotification(address_lowercase);
    batteries_.erase(it);
  }
}

bool PeripheralBatteryObserver::PostNotification(const std::string& address,
                                                 const BatteryInfo& battery) {
  // Only post notification if kNotificationInterval seconds have passed since
  // last notification showed, avoiding the case where the battery level
  // oscillates around the threshold level.
  base::TimeTicks now = testing_clock_ ? testing_clock_->NowTicks() :
      base::TimeTicks::Now();
  if (now - battery.last_notification_timestamp < kNotificationInterval)
    return false;

  // Stylus battery notifications differ slightly.
  NotificationParams params = battery.is_stylus
                                  ? GetStylusNotificationParams()
                                  : GetNonStylusNotificationParams(
                                        address, battery.name, battery.level);

  auto notification = base::MakeUnique<message_center::Notification>(
      message_center::NOTIFICATION_TYPE_SIMPLE, params.id, params.title,
      params.message,
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(params.image_id),
      base::string16(), params.url,
      message_center::NotifierId(message_center::NotifierId::SYSTEM_COMPONENT,
                                 params.notifier_name),
      message_center::RichNotificationData(), nullptr);
  notification->SetSystemPriority();

  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
  return true;
}

void PeripheralBatteryObserver::CancelNotification(const std::string& address) {
  const auto it = batteries_.find(address);
  if (it != batteries_.end()) {
    std::string notification_id =
        it->second.is_stylus ? kStylusNotificationId : address;
    message_center::MessageCenter::Get()->RemoveNotification(
        notification_id, false /* by_user */);
  }
}

}  // namespace chromeos
