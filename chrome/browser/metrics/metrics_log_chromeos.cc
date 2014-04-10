// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/metrics_log_chromeos.h"

#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/common/metrics/proto/chrome_user_metrics_extension.pb.h"
#include "chrome/common/pref_names.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/screen.h"

#if defined(USE_X11)
#include "ui/events/x/touch_factory_x11.h"
#endif  // defined(USE_X11)

using metrics::ChromeUserMetricsExtension;
using metrics::PerfDataProto;
using metrics::SystemProfileProto;
typedef SystemProfileProto::Hardware::Bluetooth::PairedDevice PairedDevice;

namespace {

PairedDevice::Type AsBluetoothDeviceType(
    device::BluetoothDevice::DeviceType device_type) {
  switch (device_type) {
    case device::BluetoothDevice::DEVICE_UNKNOWN:
      return PairedDevice::DEVICE_UNKNOWN;
    case device::BluetoothDevice::DEVICE_COMPUTER:
      return PairedDevice::DEVICE_COMPUTER;
    case device::BluetoothDevice::DEVICE_PHONE:
      return PairedDevice::DEVICE_PHONE;
    case device::BluetoothDevice::DEVICE_MODEM:
      return PairedDevice::DEVICE_MODEM;
    case device::BluetoothDevice::DEVICE_AUDIO:
      return PairedDevice::DEVICE_AUDIO;
    case device::BluetoothDevice::DEVICE_CAR_AUDIO:
      return PairedDevice::DEVICE_CAR_AUDIO;
    case device::BluetoothDevice::DEVICE_VIDEO:
      return PairedDevice::DEVICE_VIDEO;
    case device::BluetoothDevice::DEVICE_PERIPHERAL:
      return PairedDevice::DEVICE_PERIPHERAL;
    case device::BluetoothDevice::DEVICE_JOYSTICK:
      return PairedDevice::DEVICE_JOYSTICK;
    case device::BluetoothDevice::DEVICE_GAMEPAD:
      return PairedDevice::DEVICE_GAMEPAD;
    case device::BluetoothDevice::DEVICE_KEYBOARD:
      return PairedDevice::DEVICE_KEYBOARD;
    case device::BluetoothDevice::DEVICE_MOUSE:
      return PairedDevice::DEVICE_MOUSE;
    case device::BluetoothDevice::DEVICE_TABLET:
      return PairedDevice::DEVICE_TABLET;
    case device::BluetoothDevice::DEVICE_KEYBOARD_MOUSE_COMBO:
      return PairedDevice::DEVICE_KEYBOARD_MOUSE_COMBO;
  }

  NOTREACHED();
  return PairedDevice::DEVICE_UNKNOWN;
}

void WriteExternalTouchscreensProto(SystemProfileProto::Hardware* hardware) {
#if defined(USE_X11)
  std::set<std::pair<int, int> > touchscreen_ids =
      ui::TouchFactory::GetInstance()->GetTouchscreenIds();
  for (std::set<std::pair<int, int> >::iterator it = touchscreen_ids.begin();
       it != touchscreen_ids.end();
       ++it) {
    SystemProfileProto::Hardware::TouchScreen* touchscreen =
        hardware->add_external_touchscreen();
    touchscreen->set_vendor_id(it->first);
    touchscreen->set_product_id(it->second);
  }
#endif  // defined(USE_X11)
}

}  // namespace

MetricsLogChromeOS::~MetricsLogChromeOS() {
}

MetricsLogChromeOS::MetricsLogChromeOS(ChromeUserMetricsExtension* uma_proto)
    : uma_proto_(uma_proto) {
  UpdateMultiProfileUserCount();
}

void MetricsLogChromeOS::LogChromeOSMetrics() {
  std::vector<PerfDataProto> perf_data;
  if (perf_provider_.GetPerfData(&perf_data)) {
    for (std::vector<PerfDataProto>::iterator iter = perf_data.begin();
         iter != perf_data.end();
         ++iter) {
      uma_proto_->add_perf_data()->Swap(&(*iter));
    }
  }

  WriteBluetoothProto();
  UpdateMultiProfileUserCount();

  SystemProfileProto::Hardware* hardware =
      uma_proto_->mutable_system_profile()->mutable_hardware();
  gfx::Display::TouchSupport has_touch = ui::GetInternalDisplayTouchSupport();
  if (has_touch == gfx::Display::TOUCH_SUPPORT_AVAILABLE)
    hardware->set_internal_display_supports_touch(true);
  else if (has_touch == gfx::Display::TOUCH_SUPPORT_UNAVAILABLE)
    hardware->set_internal_display_supports_touch(false);
  WriteExternalTouchscreensProto(hardware);
}

void MetricsLogChromeOS::WriteRealtimeStabilityAttributes(PrefService* pref) {
  SystemProfileProto::Stability* stability =
      uma_proto_->mutable_system_profile()->mutable_stability();

  int count = pref->GetInteger(prefs::kStabilityOtherUserCrashCount);
  if (count) {
    stability->set_other_user_crash_count(count);
    pref->SetInteger(prefs::kStabilityOtherUserCrashCount, 0);
  }

  count = pref->GetInteger(prefs::kStabilityKernelCrashCount);
  if (count) {
    stability->set_kernel_crash_count(count);
    pref->SetInteger(prefs::kStabilityKernelCrashCount, 0);
  }

  count = pref->GetInteger(prefs::kStabilitySystemUncleanShutdownCount);
  if (count) {
    stability->set_unclean_system_shutdown_count(count);
    pref->SetInteger(prefs::kStabilitySystemUncleanShutdownCount, 0);
  }
}

void MetricsLogChromeOS::WriteBluetoothProto() {
  SystemProfileProto::Hardware* hardware =
      uma_proto_->mutable_system_profile()->mutable_hardware();

  // BluetoothAdapterFactory::GetAdapter is synchronous on Chrome OS; if that
  // changes this will fail at the DCHECK().
  device::BluetoothAdapterFactory::GetAdapter(
      base::Bind(&MetricsLogChromeOS::SetBluetoothAdapter,
                 base::Unretained(this)));
  DCHECK(adapter_.get());

  SystemProfileProto::Hardware::Bluetooth* bluetooth =
      hardware->mutable_bluetooth();

  bluetooth->set_is_present(adapter_->IsPresent());
  bluetooth->set_is_enabled(adapter_->IsPowered());

  device::BluetoothAdapter::DeviceList devices = adapter_->GetDevices();
  for (device::BluetoothAdapter::DeviceList::iterator iter =
           devices.begin(); iter != devices.end(); ++iter) {
    device::BluetoothDevice* device = *iter;
    // Don't collect information about LE devices yet.
    if (!device->IsPaired())
      continue;

    PairedDevice* paired_device = bluetooth->add_paired_device();
    paired_device->set_bluetooth_class(device->GetBluetoothClass());
    paired_device->set_type(AsBluetoothDeviceType(device->GetDeviceType()));

    // |address| is xx:xx:xx:xx:xx:xx, extract the first three components and
    // pack into a uint32.
    std::string address = device->GetAddress();
    if (address.size() > 9 &&
        address[2] == ':' && address[5] == ':' && address[8] == ':') {
      std::string vendor_prefix_str;
      uint64 vendor_prefix;

      base::RemoveChars(address.substr(0, 9), ":", &vendor_prefix_str);
      DCHECK_EQ(6U, vendor_prefix_str.size());
      base::HexStringToUInt64(vendor_prefix_str, &vendor_prefix);

      paired_device->set_vendor_prefix(vendor_prefix);
    }

    switch (device->GetVendorIDSource()) {
      case device::BluetoothDevice::VENDOR_ID_BLUETOOTH:
        paired_device->set_vendor_id_source(PairedDevice::VENDOR_ID_BLUETOOTH);
        break;
      case device::BluetoothDevice::VENDOR_ID_USB:
        paired_device->set_vendor_id_source(PairedDevice::VENDOR_ID_USB);
        break;
      default:
        paired_device->set_vendor_id_source(PairedDevice::VENDOR_ID_UNKNOWN);
    }

    paired_device->set_vendor_id(device->GetVendorID());
    paired_device->set_product_id(device->GetProductID());
    paired_device->set_device_id(device->GetDeviceID());
  }
}

void MetricsLogChromeOS::UpdateMultiProfileUserCount() {
  metrics::SystemProfileProto* system_profile =
      uma_proto_->mutable_system_profile();

  if (chromeos::UserManager::IsInitialized() &&
      chromeos::UserManager::Get()->IsMultipleProfilesAllowed()) {
    size_t user_count = chromeos::UserManager::Get()->GetLoggedInUsers().size();

    // We invalidate the user count if it changed while the log was open.
    if (system_profile->has_multi_profile_user_count() &&
        user_count != system_profile->multi_profile_user_count()) {
      user_count = 0;
    }

    system_profile->set_multi_profile_user_count(user_count);
  }
}

void MetricsLogChromeOS::SetBluetoothAdapter(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  adapter_ = adapter;
}
