// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chromeos_metrics_provider.h"

#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/common/pref_names.h"
#include "chromeos/system/statistics_provider.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/proto/chrome_user_metrics_extension.pb.h"
#include "content/public/browser/browser_thread.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/screen.h"

#if defined(USE_X11)
#include "ui/events/x/touch_factory_x11.h"
#endif  // defined(USE_X11)

using metrics::ChromeUserMetricsExtension;
using metrics::SampledProfile;
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

void IncrementPrefValue(const char* path) {
  PrefService* pref = g_browser_process->local_state();
  DCHECK(pref);
  int value = pref->GetInteger(path);
  pref->SetInteger(path, value + 1);
}

}  // namespace

ChromeOSMetricsProvider::ChromeOSMetricsProvider()
    : registered_user_count_at_log_initialization_(false),
      user_count_at_log_initialization_(0),
      weak_ptr_factory_(this) {
}

ChromeOSMetricsProvider::~ChromeOSMetricsProvider() {
}

// static
void ChromeOSMetricsProvider::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kStabilityOtherUserCrashCount, 0);
  registry->RegisterIntegerPref(prefs::kStabilityKernelCrashCount, 0);
  registry->RegisterIntegerPref(prefs::kStabilitySystemUncleanShutdownCount, 0);
}

// static
void ChromeOSMetricsProvider::LogCrash(const std::string& crash_type) {
  if (crash_type == "user")
    IncrementPrefValue(prefs::kStabilityOtherUserCrashCount);
  else if (crash_type == "kernel")
    IncrementPrefValue(prefs::kStabilityKernelCrashCount);
  else if (crash_type == "uncleanshutdown")
    IncrementPrefValue(prefs::kStabilitySystemUncleanShutdownCount);
  else
    NOTREACHED() << "Unexpected Chrome OS crash type " << crash_type;

  // Wake up metrics logs sending if necessary now that new
  // log data is available.
  g_browser_process->metrics_service()->OnApplicationNotIdle();
}

void ChromeOSMetricsProvider::OnDidCreateMetricsLog() {
  registered_user_count_at_log_initialization_ = false;
  if (chromeos::UserManager::IsInitialized()) {
    registered_user_count_at_log_initialization_ = true;
    user_count_at_log_initialization_ =
        chromeos::UserManager::Get()->GetLoggedInUsers().size();
  }
}

void ChromeOSMetricsProvider::InitTaskGetHardwareClass(
    const base::Closure& callback) {
  // Run the (potentially expensive) task on the FILE thread to avoid blocking
  // the UI thread.
  content::BrowserThread::PostTaskAndReply(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&ChromeOSMetricsProvider::InitTaskGetHardwareClassOnFileThread,
                 weak_ptr_factory_.GetWeakPtr()),
      callback);
}

void ChromeOSMetricsProvider::InitTaskGetHardwareClassOnFileThread() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
  chromeos::system::StatisticsProvider::GetInstance()->GetMachineStatistic(
      "hardware_class", &hardware_class_);
}

void ChromeOSMetricsProvider::ProvideSystemProfileMetrics(
    metrics::SystemProfileProto* system_profile_proto) {
  WriteBluetoothProto(system_profile_proto);
  UpdateMultiProfileUserCount(system_profile_proto);

  metrics::SystemProfileProto::Hardware* hardware =
      system_profile_proto->mutable_hardware();
  hardware->set_hardware_class(hardware_class_);
  gfx::Display::TouchSupport has_touch = ui::GetInternalDisplayTouchSupport();
  if (has_touch == gfx::Display::TOUCH_SUPPORT_AVAILABLE)
    hardware->set_internal_display_supports_touch(true);
  else if (has_touch == gfx::Display::TOUCH_SUPPORT_UNAVAILABLE)
    hardware->set_internal_display_supports_touch(false);
  WriteExternalTouchscreensProto(hardware);
}

void ChromeOSMetricsProvider::ProvideStabilityMetrics(
    metrics::SystemProfileProto* system_profile_proto) {
  metrics::SystemProfileProto::Stability* stability_proto =
      system_profile_proto->mutable_stability();
  PrefService* pref = g_browser_process->local_state();
  int count = pref->GetInteger(prefs::kStabilityOtherUserCrashCount);
  if (count) {
    stability_proto->set_other_user_crash_count(count);
    pref->SetInteger(prefs::kStabilityOtherUserCrashCount, 0);
  }

  count = pref->GetInteger(prefs::kStabilityKernelCrashCount);
  if (count) {
    stability_proto->set_kernel_crash_count(count);
    pref->SetInteger(prefs::kStabilityKernelCrashCount, 0);
  }

  count = pref->GetInteger(prefs::kStabilitySystemUncleanShutdownCount);
  if (count) {
    stability_proto->set_unclean_system_shutdown_count(count);
    pref->SetInteger(prefs::kStabilitySystemUncleanShutdownCount, 0);
  }
}

void ChromeOSMetricsProvider::ProvideGeneralMetrics(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  std::vector<SampledProfile> sampled_profiles;
  if (perf_provider_.GetSampledProfiles(&sampled_profiles)) {
    for (std::vector<SampledProfile>::iterator iter = sampled_profiles.begin();
         iter != sampled_profiles.end();
         ++iter) {
      uma_proto->add_sampled_profile()->Swap(&(*iter));
    }
  }
}

void ChromeOSMetricsProvider::WriteBluetoothProto(
    metrics::SystemProfileProto* system_profile_proto) {
  metrics::SystemProfileProto::Hardware* hardware =
      system_profile_proto->mutable_hardware();

  // BluetoothAdapterFactory::GetAdapter is synchronous on Chrome OS; if that
  // changes this will fail at the DCHECK().
  device::BluetoothAdapterFactory::GetAdapter(base::Bind(
      &ChromeOSMetricsProvider::SetBluetoothAdapter, base::Unretained(this)));
  DCHECK(adapter_.get());

  SystemProfileProto::Hardware::Bluetooth* bluetooth =
      hardware->mutable_bluetooth();

  bluetooth->set_is_present(adapter_->IsPresent());
  bluetooth->set_is_enabled(adapter_->IsPowered());

  device::BluetoothAdapter::DeviceList devices = adapter_->GetDevices();
  for (device::BluetoothAdapter::DeviceList::iterator iter = devices.begin();
       iter != devices.end();
       ++iter) {
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
    if (address.size() > 9 && address[2] == ':' && address[5] == ':' &&
        address[8] == ':') {
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

void ChromeOSMetricsProvider::UpdateMultiProfileUserCount(
    metrics::SystemProfileProto* system_profile_proto) {
  if (chromeos::UserManager::IsInitialized()) {
    size_t user_count = chromeos::UserManager::Get()->GetLoggedInUsers().size();

    // We invalidate the user count if it changed while the log was open.
    if (registered_user_count_at_log_initialization_ &&
        user_count != user_count_at_log_initialization_) {
      user_count = 0;
    }

    system_profile_proto->set_multi_profile_user_count(user_count);
  }
}

void ChromeOSMetricsProvider::SetBluetoothAdapter(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  adapter_ = adapter;
}
