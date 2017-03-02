// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/peripheral_battery_observer.h"

#include <vector>

#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/grit/theme_resources.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "content/public/browser/browser_thread.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

namespace chromeos {

namespace {

// When a peripheral device's battery level is <= kLowBatteryLevel, consider
// it to be in low battery condition.
const int kLowBatteryLevel = 15;

// Don't show 2 low battery notification within |kNotificationIntervalSec|
// seconds.
const int kNotificationIntervalSec = 60;

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
  std::vector<std::string> result = base::SplitString(
      reverse_address, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  std::reverse(result.begin(), result.end());
  std::string address = base::JoinString(result, ":");
  return address;
}

class PeripheralBatteryNotificationDelegate : public NotificationDelegate {
 public:
  explicit PeripheralBatteryNotificationDelegate(const std::string& id)
      : id_(id) {}

  // Overridden from NotificationDelegate:
  std::string id() const override { return id_; }

 private:
  ~PeripheralBatteryNotificationDelegate() override {}

  const std::string id_;

  DISALLOW_COPY_AND_ASSIGN(PeripheralBatteryNotificationDelegate);
};

}  // namespace

PeripheralBatteryObserver::PeripheralBatteryObserver()
    : testing_clock_(NULL),
      notification_profile_(NULL),
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
    BatteryInfo battery(name, level, base::TimeTicks());
    if (level <= kLowBatteryLevel) {
      if (PostNotification(address, battery))
        battery.last_notification_timestamp = testing_clock_ ?
            testing_clock_->NowTicks() : base::TimeTicks::Now();
    }
    batteries_[address] = battery;
  } else {
    BatteryInfo* battery = &batteries_[address];
    battery->name = name;
    int old_level = battery->level;
    battery->level = level;
    if (old_level > kLowBatteryLevel && level <= kLowBatteryLevel) {
      if (PostNotification(address, *battery))
        battery->last_notification_timestamp = testing_clock_ ?
            testing_clock_->NowTicks() : base::TimeTicks::Now();
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
    batteries_.erase(it);
    CancelNotification(address_lowercase);
  }
}

bool PeripheralBatteryObserver::PostNotification(const std::string& address,
                                                 const BatteryInfo& battery) {
  // Only post notification if kNotificationInterval seconds have passed since
  // last notification showed, avoiding the case where the battery level
  // oscillates around the threshold level.
  base::TimeTicks now = testing_clock_ ? testing_clock_->NowTicks() :
      base::TimeTicks::Now();
  if (now - battery.last_notification_timestamp <
      base::TimeDelta::FromSeconds(kNotificationIntervalSec))
    return false;

  NotificationUIManager* notification_manager =
      g_browser_process->notification_ui_manager();

  base::string16 string_text = l10n_util::GetStringFUTF16Int(
      IDS_ASH_LOW_PERIPHERAL_BATTERY_NOTIFICATION_TEXT,
      battery.level);

  Notification notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, base::UTF8ToUTF16(battery.name),
      string_text, ui::ResourceBundle::GetSharedInstance().GetImageNamed(
                       IDR_NOTIFICATION_PERIPHERAL_BATTERY_LOW),
      message_center::NotifierId(message_center::NotifierId::SYSTEM_COMPONENT,
                                 kNotifierId),
      base::string16(), GURL(kNotificationOriginUrl), address,
      message_center::RichNotificationData(),
      new PeripheralBatteryNotificationDelegate(address));

  notification.set_priority(message_center::SYSTEM_PRIORITY);

  notification_profile_ = ProfileManager::GetPrimaryUserProfile();
  notification_manager->Add(notification, notification_profile_);

  return true;
}

void PeripheralBatteryObserver::CancelNotification(const std::string& address) {
  // If last_used_profile_ is NULL then no notification has been posted yet.
  if (notification_profile_) {
    g_browser_process->notification_ui_manager()->CancelById(
        address, NotificationUIManager::GetProfileID(notification_profile_));
  }
}

}  // namespace chromeos
