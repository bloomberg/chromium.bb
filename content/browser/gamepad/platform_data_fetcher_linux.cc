// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gamepad/platform_data_fetcher_linux.h"

#include <dlfcn.h>
#include <fcntl.h>
#include <libudev.h>
#include <linux/joystick.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "base/debug/trace_event.h"
#include "base/eintr_wrapper.h"
#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "content/common/gamepad_hardware_buffer.h"

namespace content {

using WebKit::WebGamepad;
using WebKit::WebGamepads;

GamepadPlatformDataFetcherLinux::GamepadPlatformDataFetcherLinux() {
  for (size_t i = 0; i < arraysize(device_fds_); ++i)
    device_fds_[i] = -1;
  memset(mappers_, 0, sizeof(mappers_));

  udev_ = udev_new();

  monitor_ = udev_monitor_new_from_netlink(udev_, "udev");
  udev_monitor_filter_add_match_subsystem_devtype(monitor_, "input", NULL);
  udev_monitor_enable_receiving(monitor_);
  monitor_fd_ = udev_monitor_get_fd(monitor_);
  MessageLoopForIO::current()->WatchFileDescriptor(monitor_fd_, true,
      MessageLoopForIO::WATCH_READ, &monitor_watcher_, this);

  EnumerateDevices();
}

GamepadPlatformDataFetcherLinux::~GamepadPlatformDataFetcherLinux() {
  monitor_watcher_.StopWatchingFileDescriptor();
  udev_unref(udev_);
  for (size_t i = 0; i < WebGamepads::itemsLengthCap; ++i) {
    if (device_fds_[i] >= 0)
      close(device_fds_[i]);
  }
}

void GamepadPlatformDataFetcherLinux::GetGamepadData(WebGamepads* pads, bool) {
  TRACE_EVENT0("GAMEPAD", "GetGamepadData");

  data_.length = WebGamepads::itemsLengthCap;

  // Update our internal state.
  for (size_t i = 0; i < WebGamepads::itemsLengthCap; ++i) {
    if (device_fds_[i] >= 0) {
      ReadDeviceData(i);
    }
  }

  // Copy to the current state to the output buffer, using the mapping
  // function, if there is one available.
  pads->length = data_.length;
  for (size_t i = 0; i < WebGamepads::itemsLengthCap; ++i) {
    if (mappers_[i])
      mappers_[i](data_.items[i], &pads->items[i]);
    else
      pads->items[i] = data_.items[i];
  }
}

void GamepadPlatformDataFetcherLinux::OnFileCanReadWithoutBlocking(int fd) {
  // Events occur when devices attached to the system are added, removed, or
  // change state. udev_monitor_receive_device() will return a device object
  // representing the device which changed and what type of change occured.
  DCHECK(monitor_fd_ == fd);
  udev_device* dev = udev_monitor_receive_device(monitor_);
  RefreshDevice(dev);
  udev_device_unref(dev);
}

void GamepadPlatformDataFetcherLinux::OnFileCanWriteWithoutBlocking(int fd) {
}

bool GamepadPlatformDataFetcherLinux::IsGamepad(
    udev_device* dev,
    int& index,
    std::string& path) {
  if (!udev_device_get_property_value(dev, "ID_INPUT_JOYSTICK"))
    return false;

  const char* node_path = udev_device_get_devnode(dev);
  if (!node_path)
    return false;

  static const char kJoystickRoot[] = "/dev/input/js";
  bool is_gamepad =
      strncmp(kJoystickRoot, node_path, sizeof(kJoystickRoot) - 1) == 0;
  if (is_gamepad) {
    const int base_len = sizeof(kJoystickRoot) - 1;
    if (!base::StringToInt(base::StringPiece(
            &node_path[base_len],
            strlen(node_path) - base_len),
        &index))
      return false;
    if (index < 0 || index >= static_cast<int>(WebGamepads::itemsLengthCap))
      return false;
    path = std::string(node_path);
  }
  return is_gamepad;
}

// Used during enumeration, and monitor notifications.
void GamepadPlatformDataFetcherLinux::RefreshDevice(udev_device* dev) {
  int index;
  std::string node_path;
  if (IsGamepad(dev, index, node_path)) {
    int& device_fd = device_fds_[index];
    WebGamepad& pad = data_.items[index];
    GamepadStandardMappingFunction& mapper = mappers_[index];

    if (device_fd >= 0)
      close(device_fd);

    // The device pointed to by dev contains information about the input
    // device. In order to get the information about the USB device, get the
    // parent device with the subsystem/devtype pair of "usb"/"usb_device".
    // This function walks up the tree several levels.
    dev = udev_device_get_parent_with_subsystem_devtype(
        dev,
        "usb",
        "usb_device");
    if (!dev) {
      // Unable to get device information, don't use this device.
      device_fd = -1;
      pad.connected = false;
      return;
    }

    device_fd = HANDLE_EINTR(open(node_path.c_str(), O_RDONLY | O_NONBLOCK));
    if (device_fd < 0) {
      // Unable to open device, don't use.
      pad.connected = false;
      return;
    }

    const char* vendor_id = udev_device_get_sysattr_value(dev, "idVendor");
    const char* product_id = udev_device_get_sysattr_value(dev, "idProduct");
    mapper = GetGamepadStandardMappingFunction(vendor_id, product_id);

    const char* manufacturer =
        udev_device_get_sysattr_value(dev, "manufacturer");
    const char* product = udev_device_get_sysattr_value(dev, "product");

    // Driver returns utf-8 strings here, so combine in utf-8 and then convert
    // to WebUChar to build the id string.
    std::string id;
    if (mapper) {
      id = base::StringPrintf("%s %s (STANDARD GAMEPAD)",
          manufacturer,
          product);
    } else {
      id = base::StringPrintf("%s %s (Vendor: %s Product: %s)",
          manufacturer,
          product,
          vendor_id,
          product_id);
    }
    TruncateUTF8ToByteSize(id, WebGamepad::idLengthCap - 1, &id);
    string16 tmp16 = UTF8ToUTF16(id);
    memset(pad.id, 0, sizeof(pad.id));
    tmp16.copy(pad.id, arraysize(pad.id) - 1);

    pad.connected = true;
  }
}

void GamepadPlatformDataFetcherLinux::EnumerateDevices() {
  udev_enumerate* enumerate = udev_enumerate_new(udev_);
  udev_enumerate_add_match_subsystem(enumerate, "input");
  udev_enumerate_scan_devices(enumerate);

  udev_list_entry* devices = udev_enumerate_get_list_entry(enumerate);
  for (udev_list_entry* dev_list_entry = devices;
       dev_list_entry != NULL;
       dev_list_entry = udev_list_entry_get_next(dev_list_entry)) {
    // Get the filename of the /sys entry for the device and create a
    // udev_device object (dev) representing it
    const char* path = udev_list_entry_get_name(dev_list_entry);
    udev_device* dev = udev_device_new_from_syspath(udev_, path);
    RefreshDevice(dev);
    udev_device_unref(dev);
  }
  // Free the enumerator object
  udev_enumerate_unref(enumerate);
}

void GamepadPlatformDataFetcherLinux::ReadDeviceData(size_t index) {
  int& fd = device_fds_[index];
  WebGamepad& pad = data_.items[index];
  DCHECK(fd >= 0);

  js_event event;
  while (HANDLE_EINTR(read(fd, &event, sizeof(struct js_event)) > 0)) {
    size_t item = event.number;
    if (event.type & JS_EVENT_AXIS) {
      if (item >= WebGamepad::axesLengthCap)
        continue;
      pad.axes[item] = event.value / 32767.f;
      if (item >= pad.axesLength)
        pad.axesLength = item + 1;
    } else if (event.type & JS_EVENT_BUTTON) {
      if (item >= WebGamepad::buttonsLengthCap)
        continue;
      pad.buttons[item] = event.value ? 1.0 : 0.0;
      if (item >= pad.buttonsLength)
        pad.buttonsLength = item + 1;
    }
    pad.timestamp = event.time;
  }
}


}  // namespace content
