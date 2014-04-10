// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <libudev.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/platform_file.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "device/hid/hid_connection_linux.h"
#include "device/hid/hid_device_info.h"
#include "device/hid/hid_service_linux.h"
#include "device/hid/udev_common.h"

namespace device {

namespace {

const char kHIDSubSystem[] = "hid";

const char kHIDID[] = "HID_ID";
const char kHIDName[] = "HID_NAME";
const char kHIDUnique[] = "HID_UNIQ";

} // namespace

HidServiceLinux::HidServiceLinux() {
  DeviceMonitorLinux* monitor = DeviceMonitorLinux::GetInstance();
  monitor->AddObserver(this);
  monitor->Enumerate(
      base::Bind(&HidServiceLinux::OnDeviceAdded, base::Unretained(this)));
}

scoped_refptr<HidConnection> HidServiceLinux::Connect(
    const HidDeviceId& device_id) {
  HidDeviceInfo device_info;
  if (!GetDeviceInfo(device_id, &device_info))
    return NULL;

  ScopedUdevDevicePtr device =
      DeviceMonitorLinux::GetInstance()->GetDeviceFromPath(
          device_info.device_id);
  if (device)
    return new HidConnectionLinux(device_info, device.Pass());
  return NULL;
}

HidServiceLinux::~HidServiceLinux() {
  if (DeviceMonitorLinux::HasInstance())
    DeviceMonitorLinux::GetInstance()->RemoveObserver(this);
}

void HidServiceLinux::OnDeviceAdded(udev_device* device) {
  if (!device)
    return;

  const char* device_path = udev_device_get_syspath(device);
  if (!device_path)
    return;
  const char* subsystem = udev_device_get_subsystem(device);
  if (!subsystem || strcmp(subsystem, kHIDSubSystem) != 0)
    return;

  HidDeviceInfo device_info;
  device_info.device_id = device_path;

  uint32_t int_property = 0;
  const char* str_property = NULL;

  const char* hid_id = udev_device_get_property_value(device, kHIDID);
  if (!hid_id)
    return;

  std::vector<std::string> parts;
  base::SplitString(hid_id, ':', &parts);
  if (parts.size() != 3) {
    return;
  }

  if (HexStringToUInt(base::StringPiece(parts[1]), &int_property)) {
    device_info.vendor_id = int_property;
  }

  if (HexStringToUInt(base::StringPiece(parts[2]), &int_property)) {
    device_info.product_id = int_property;
  }

  str_property = udev_device_get_property_value(device, kHIDUnique);
  if (str_property != NULL)
    device_info.serial_number = str_property;

  str_property = udev_device_get_property_value(device, kHIDName);
  if (str_property != NULL)
    device_info.product_name = str_property;

  AddDevice(device_info);
}

void HidServiceLinux::OnDeviceRemoved(udev_device* device) {
  const char* device_path = udev_device_get_syspath(device);;
  if (device_path)
    RemoveDevice(device_path);
}

} // namespace dev
