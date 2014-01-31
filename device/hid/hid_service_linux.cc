// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <libudev.h>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_vector.h"
#include "base/platform_file.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/threading/thread_restrictions.h"
#include "device/hid/hid_connection.h"
#include "device/hid/hid_connection_linux.h"
#include "device/hid/hid_device_info.h"
#include "device/hid/hid_service_linux.h"

namespace device {

namespace {

const char kUdevName[] = "udev";
const char kUdevActionAdd[] = "add";
const char kUdevActionRemove[] = "remove";
const char kHIDSubSystem[] = "hid";

const char kHIDID[] = "HID_ID";
const char kHIDName[] = "HID_NAME";
const char kHIDUnique[] = "HID_UNIQ";

} // namespace

HidServiceLinux::HidServiceLinux() {
  udev_.reset(udev_new());
  if (!udev_) {
    LOG(ERROR) << "Failed to create udev.";
    return;
  }
  monitor_.reset(udev_monitor_new_from_netlink(udev_.get(), kUdevName));
  if (!monitor_) {
    LOG(ERROR) << "Failed to create udev monitor.";
    return;
  }
  int ret = udev_monitor_filter_add_match_subsystem_devtype(
      monitor_.get(),
      kHIDSubSystem,
      NULL);
  if (ret != 0) {
    LOG(ERROR) << "Failed to add udev monitor filter.";
    return;
  }

  ret = udev_monitor_enable_receiving(monitor_.get());
  if (ret != 0) {
    LOG(ERROR) << "Failed to start udev monitoring.";
    return;
  }

  monitor_fd_ = udev_monitor_get_fd(monitor_.get());
  if (monitor_fd_ <= 0) {
    LOG(ERROR) << "Failed to start udev monitoring.";
    return;
  }

  if (!base::MessageLoopForIO::current()->WatchFileDescriptor(
      monitor_fd_,
      true,
      base::MessageLoopForIO::WATCH_READ,
      &monitor_watcher_,
      this))
    return;

  Enumerate();
}

HidServiceLinux::~HidServiceLinux() {
  monitor_watcher_.StopWatchingFileDescriptor();
  close(monitor_fd_);
}

void HidServiceLinux::Enumerate() {
  scoped_ptr<udev_enumerate, UdevEnumerateDeleter> enumerate(
      udev_enumerate_new(udev_.get()));

  if (!enumerate) {
    LOG(ERROR) << "Failed to enumerate devices.";
    return;
  }

  if (udev_enumerate_add_match_subsystem(enumerate.get(), kHIDSubSystem)) {
    LOG(ERROR) << "Failed to enumerate devices.";
    return;
  }

  if (udev_enumerate_scan_devices(enumerate.get()) != 0) {
    LOG(ERROR) << "Failed to enumerate devices.";
    return;
  }

  // This list is managed by |enumerate|.
  udev_list_entry* devices = udev_enumerate_get_list_entry(enumerate.get());
  for (udev_list_entry* i = devices; i != NULL;
      i = udev_list_entry_get_next(i)) {
    ScopedUdevDevicePtr hid_dev(
        udev_device_new_from_syspath(udev_.get(), udev_list_entry_get_name(i)));
    if (hid_dev) {
      PlatformDeviceAdd(hid_dev.get());
    }
  }

  initialized_ = true;
}

void HidServiceLinux::PlatformDeviceAdd(udev_device* device) {
  if (!device)
    return;

  const char* device_id = udev_device_get_syspath(device);
  if (!device_id)
    return;


  HidDeviceInfo device_info;
  device_info.device_id = device_id;

  uint32 int_property = 0;
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

void HidServiceLinux::PlatformDeviceRemove(udev_device* raw_dev) {
  // The returned the device is not referenced.
  udev_device* hid_dev =
      udev_device_get_parent_with_subsystem_devtype(raw_dev, "hid", NULL);

  if (!hid_dev)
    return;

  const char* device_id = NULL;
  device_id = udev_device_get_syspath(hid_dev);
  if (device_id == NULL)
    return;

  RemoveDevice(device_id);
}

scoped_refptr<HidConnection> HidServiceLinux::Connect(std::string device_id) {
  if (!ContainsKey(devices_, device_id))
    return NULL;
  ScopedUdevDevicePtr hid_device(
      udev_device_new_from_syspath(udev_.get(), device_id.c_str()));
  if (hid_device) {
    scoped_refptr<HidConnectionLinux> connection =
        new HidConnectionLinux(devices_[device_id], hid_device.Pass());
    if (connection->initialized())
      return connection;
  }
  return NULL;
}

void HidServiceLinux::OnFileCanReadWithoutBlocking(int fd) {
  DCHECK_EQ(monitor_fd_, fd);

  ScopedUdevDevicePtr dev(udev_monitor_receive_device(monitor_.get()));
  if (!dev)
    return;

  std::string action(udev_device_get_action(dev.get()));
  if (action == kUdevActionAdd) {
    PlatformDeviceAdd(dev.get());
  } else if (action == kUdevActionRemove) {
    PlatformDeviceRemove(dev.get());
  }
}

void HidServiceLinux::OnFileCanWriteWithoutBlocking(int fd) {}

} // namespace dev
