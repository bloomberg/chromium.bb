// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/hid/hid_service_linux.h"

#include <string>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread_restrictions.h"
#include "device/hid/hid_connection_linux.h"
#include "device/hid/hid_device_info.h"
#include "device/hid/hid_report_descriptor.h"
#include "device/udev_linux/udev.h"

#if defined(OS_CHROMEOS)
#include "base/sys_info.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/permission_broker_client.h"
#endif  // defined(OS_CHROMEOS)

namespace device {

namespace {

const char kHidrawSubsystem[] = "hidraw";
const char kHIDID[] = "HID_ID";
const char kHIDName[] = "HID_NAME";
const char kHIDUnique[] = "HID_UNIQ";
const char kSysfsReportDescriptorKey[] = "report_descriptor";

}  // namespace

HidServiceLinux::HidServiceLinux(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner)
    : ui_task_runner_(ui_task_runner),
      weak_factory_(this) {
  base::ThreadRestrictions::AssertIOAllowed();
  task_runner_ = base::ThreadTaskRunnerHandle::Get();
  DeviceMonitorLinux* monitor = DeviceMonitorLinux::GetInstance();
  monitor->AddObserver(this);
  monitor->Enumerate(
      base::Bind(&HidServiceLinux::OnDeviceAdded, weak_factory_.GetWeakPtr()));
}

#if defined(OS_CHROMEOS)
void HidServiceLinux::RequestAccess(
    const HidDeviceId& device_id,
    const base::Callback<void(bool success)>& callback) {
  bool success = false;
  ScopedUdevDevicePtr device =
      DeviceMonitorLinux::GetInstance()->GetDeviceFromPath(
          device_id);

  if (device) {
    const char* dev_node = udev_device_get_devnode(device.get());

    if (base::SysInfo::IsRunningOnChromeOS()) {
      chromeos::PermissionBrokerClient* client =
          chromeos::DBusThreadManager::Get()->GetPermissionBrokerClient();
      DCHECK(client) << "Could not get permission broker client.";
      if (client) {
        ui_task_runner_->PostTask(
            FROM_HERE,
            base::Bind(&chromeos::PermissionBrokerClient::RequestPathAccess,
                       base::Unretained(client),
                       std::string(dev_node),
                       -1,
                       base::Bind(&HidServiceLinux::OnRequestAccessComplete,
                                  weak_factory_.GetWeakPtr(),
                                  callback)));
        return;
      }
    } else {
      // Not really running on Chrome OS, declare success.
      success = true;
    }
  }
  task_runner_->PostTask(FROM_HERE, base::Bind(callback, success));
}
#endif

scoped_refptr<HidConnection> HidServiceLinux::Connect(
    const HidDeviceId& device_id) {
  HidDeviceInfo device_info;
  if (!GetDeviceInfo(device_id, &device_info))
    return NULL;

  ScopedUdevDevicePtr device =
      DeviceMonitorLinux::GetInstance()->GetDeviceFromPath(
          device_info.device_id);

  if (device) {
    const char* dev_node = udev_device_get_devnode(device.get());
    if (dev_node) {
      return new HidConnectionLinux(device_info, dev_node);
    }
  }

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
  if (!subsystem || strcmp(subsystem, kHidrawSubsystem) != 0)
    return;

  HidDeviceInfo device_info;
  device_info.device_id = device_path;

  uint32_t int_property = 0;
  const char* str_property = NULL;

  udev_device *parent = udev_device_get_parent(device);
  if (!parent) {
    return;
  }

  const char* hid_id = udev_device_get_property_value(parent, kHIDID);
  if (!hid_id) {
    return;
  }

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

  str_property = udev_device_get_property_value(parent, kHIDUnique);
  if (str_property != NULL) {
    device_info.serial_number = str_property;
  }

  str_property = udev_device_get_property_value(parent, kHIDName);
  if (str_property != NULL) {
    device_info.product_name = str_property;
  }

  const char* parent_sysfs_path = udev_device_get_syspath(parent);
  if (!parent_sysfs_path) {
    return;
  }
  base::FilePath report_descriptor_path =
      base::FilePath(parent_sysfs_path).Append(kSysfsReportDescriptorKey);
  std::string report_descriptor_str;
  if (!base::ReadFileToString(report_descriptor_path, &report_descriptor_str)) {
    return;
  }

  HidReportDescriptor report_descriptor(
      reinterpret_cast<uint8_t*>(&report_descriptor_str[0]),
      report_descriptor_str.length());
  report_descriptor.GetDetails(&device_info.collections,
                               &device_info.has_report_id,
                               &device_info.max_input_report_size,
                               &device_info.max_output_report_size,
                               &device_info.max_feature_report_size);

  AddDevice(device_info);
}

void HidServiceLinux::OnDeviceRemoved(udev_device* device) {
  const char* device_path = udev_device_get_syspath(device);;
  if (device_path) {
    RemoveDevice(device_path);
  }
}

void HidServiceLinux::OnRequestAccessComplete(
    const base::Callback<void(bool success)>& callback,
    bool success) {
  task_runner_->PostTask(FROM_HERE, base::Bind(callback, success));
}

}  // namespace device
