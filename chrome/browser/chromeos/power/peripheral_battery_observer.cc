// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/peripheral_battery_observer.h"

#include <vector>

#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/bind.h"
#include "base/strings/string16.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
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

// HID Bluetooth device's battery sysfs entry path looks like
// "/sys/class/power_supply/hid-AA:BB:CC:DD:EE:FF-battery".
// Here the bluetooth address is showed in reverse order and its true
// address "FF:EE:DD:CC:BB:AA".
const char kHIDBatteryPathPrefix[] = "/sys/class/power_supply/hid-";
const char kHIDBatteryPathSuffix[] = "-battery";

bool IsBluetoothHIDBattery(const std::string& path) {
  return StartsWithASCII(path, kHIDBatteryPathPrefix, false) &&
      EndsWith(path, kHIDBatteryPathSuffix, false);
}

std::string ExtractBluetoothAddress(const std::string& path) {
  int header_size = strlen(kHIDBatteryPathPrefix);
  int end_size = strlen(kHIDBatteryPathSuffix);
  int key_len = path.size() - header_size - end_size;
  if (key_len <= 0)
    return std::string();
  std::string reverse_address = path.substr(header_size, key_len);
  base::StringToLowerASCII(&reverse_address);
  std::vector<std::string> result;
  base::SplitString(reverse_address, ':', &result);
  std::reverse(result.begin(), result.end());
  std::string address = JoinString(result, ':');
  return address;
}

class PeripheralBatteryNotificationDelegate : public NotificationDelegate {
 public:
  explicit PeripheralBatteryNotificationDelegate(const std::string& id)
      : id_(id) {}

  // Overridden from NotificationDelegate:
  virtual void Display() OVERRIDE {}
  virtual void Error() OVERRIDE {}
  virtual void Close(bool by_user) OVERRIDE {}
  virtual void Click() OVERRIDE {}
  virtual std::string id() const OVERRIDE { return id_; }
  // A NULL return value prevents loading image from URL. It is OK since our
  // implementation loads image from system resource bundle.
  virtual content::WebContents* GetWebContents() const OVERRIDE {
    return NULL;
  }

 private:
  virtual ~PeripheralBatteryNotificationDelegate() {}

  const std::string id_;

  DISALLOW_COPY_AND_ASSIGN(PeripheralBatteryNotificationDelegate);
};

}  // namespace

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
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
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
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  std::string address_lowercase = address;
  base::StringToLowerASCII(&address_lowercase);
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
      // TODO(mukai): add SYSTEM priority and use here.
      GURL(kNotificationOriginUrl),
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          IDR_NOTIFICATION_PERIPHERAL_BATTERY_LOW),
      base::UTF8ToUTF16(battery.name),
      string_text,
      blink::WebTextDirectionDefault,
      base::string16(),
      base::UTF8ToUTF16(address),
      new PeripheralBatteryNotificationDelegate(address));

  notification_manager->Add(
      notification,
      ProfileManager::GetPrimaryUserProfile());

  return true;
}

void PeripheralBatteryObserver::CancelNotification(const std::string& address) {
  g_browser_process->notification_ui_manager()->CancelById(address);
}

}  // namespace chromeos
