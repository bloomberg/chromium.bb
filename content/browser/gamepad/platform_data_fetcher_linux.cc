// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gamepad/platform_data_fetcher_linux.h"

#include <fcntl.h>
#include <libudev.h>
#include <linux/joystick.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/debug/trace_event.h"
#include "base/eintr_wrapper.h"
#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"

namespace {

const char kInputSubsystem[] = "input";
const char kUsbSubsystem[] = "usb";
const char kUsbDeviceType[] = "usb_device";
const float kMaxLinuxAxisValue = 32767.0;

void CloseFileDescriptorIfValid(int fd) {
  if (fd >= 0)
    close(fd);
}

bool IsGamepad(udev_device* dev, int* index, std::string* path) {
  if (!udev_device_get_property_value(dev, "ID_INPUT_JOYSTICK"))
    return false;

  const char* node_path = udev_device_get_devnode(dev);
  if (!node_path)
    return false;

  static const char kJoystickRoot[] = "/dev/input/js";
  bool is_gamepad = StartsWithASCII(node_path, kJoystickRoot, true);
  if (!is_gamepad)
    return false;

  int tmp_idx = -1;
  const int base_len = sizeof(kJoystickRoot) - 1;
  base::StringPiece str(&node_path[base_len], strlen(node_path) - base_len);
  if (!base::StringToInt(str, &tmp_idx))
    return false;
  if (tmp_idx < 0 ||
      tmp_idx >= static_cast<int>(WebKit::WebGamepads::itemsLengthCap)) {
    return false;
  }
  *index = tmp_idx;
  *path = node_path;
  return true;
}

}  // namespace

namespace content {

using WebKit::WebGamepad;
using WebKit::WebGamepads;

GamepadPlatformDataFetcherLinux::GamepadPlatformDataFetcherLinux() {
  for (size_t i = 0; i < arraysize(device_fds_); ++i)
    device_fds_[i] = -1;
  memset(mappers_, 0, sizeof(mappers_));

  udev_ = udev_new();
  CHECK(udev_);

  monitor_ = udev_monitor_new_from_netlink(udev_, "udev");
  CHECK(monitor_);
  int ret = udev_monitor_filter_add_match_subsystem_devtype(monitor_,
                                                            kInputSubsystem,
                                                            NULL);
  CHECK_EQ(0, ret);
  ret = udev_monitor_enable_receiving(monitor_);
  CHECK_EQ(0, ret);
  monitor_fd_ = udev_monitor_get_fd(monitor_);
  CHECK_GE(monitor_fd_, 0);
  bool success = MessageLoopForIO::current()->WatchFileDescriptor(monitor_fd_,
      true, MessageLoopForIO::WATCH_READ, &monitor_watcher_, this);
  CHECK(success);

  EnumerateDevices();
}

GamepadPlatformDataFetcherLinux::~GamepadPlatformDataFetcherLinux() {
  monitor_watcher_.StopWatchingFileDescriptor();
  udev_monitor_unref(monitor_);
  udev_unref(udev_);
  for (size_t i = 0; i < WebGamepads::itemsLengthCap; ++i)
    CloseFileDescriptorIfValid(device_fds_[i]);
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
  DCHECK_EQ(monitor_fd_, fd);
  udev_device* dev = udev_monitor_receive_device(monitor_);
  if (!dev)
    return;
  RefreshDevice(dev);
  udev_device_unref(dev);
}

void GamepadPlatformDataFetcherLinux::OnFileCanWriteWithoutBlocking(int fd) {
}

// Used during enumeration, and monitor notifications.
void GamepadPlatformDataFetcherLinux::RefreshDevice(udev_device* dev) {
  int index;
  std::string node_path;
  if (IsGamepad(dev, &index, &node_path)) {
    int& device_fd = device_fds_[index];
    WebGamepad& pad = data_.items[index];
    GamepadStandardMappingFunction& mapper = mappers_[index];

    CloseFileDescriptorIfValid(device_fd);

    // The device pointed to by dev contains information about the logical
    // joystick device. In order to get the information about the physical
    // hardware, get the parent device that is also in the "input" subsystem.
    // This function should just walk up the tree one level.
    dev = udev_device_get_parent_with_subsystem_devtype(
        dev,
        kInputSubsystem,
        NULL);
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

    const char* vendor_id = udev_device_get_sysattr_value(dev, "id/vendor");
    const char* product_id = udev_device_get_sysattr_value(dev, "id/product");
    mapper = GetGamepadStandardMappingFunction(vendor_id, product_id);

    // Driver returns utf-8 strings here, so combine in utf-8 first and
    // convert to WebUChar later once we've picked an id string.
    const char* name = udev_device_get_sysattr_value(dev, "name");
    std::string name_string = base::StringPrintf("%s", name);

    // In many cases the information the input subsystem contains isn't
    // as good as the information that the device bus has, walk up further
    // to the subsystem/device type "usb"/"usb_device" and if this device
    // has the same vendor/product id, prefer the description from that.
    struct udev_device *usb_dev = udev_device_get_parent_with_subsystem_devtype(
        dev,
        kUsbSubsystem,
        kUsbDeviceType);
    if (usb_dev) {
      const char* usb_vendor_id =
          udev_device_get_sysattr_value(usb_dev, "idVendor");
      const char* usb_product_id =
          udev_device_get_sysattr_value(usb_dev, "idProduct");

      if (strcmp(vendor_id, usb_vendor_id) == 0 &&
          strcmp(product_id, usb_product_id) == 0) {
        const char* manufacturer =
            udev_device_get_sysattr_value(usb_dev, "manufacturer");
        const char* product = udev_device_get_sysattr_value(usb_dev, "product");

        // Replace the previous name string with one containing the better
        // information, again driver returns utf-8 strings here so combine
        // in utf-8 for conversion to WebUChar below.
        name_string = base::StringPrintf("%s %s", manufacturer, product);
      }
    }

    // Append the vendor and product information then convert the utf-8
    // id string to WebUChar.
    std::string id = name_string + base::StringPrintf(
        " (%sVendor: %s Product: %s)",
        mapper ? "STANDARD GAMEPAD " : "",
        vendor_id,
        product_id);
    TruncateUTF8ToByteSize(id, WebGamepad::idLengthCap - 1, &id);
    string16 tmp16 = UTF8ToUTF16(id);
    memset(pad.id, 0, sizeof(pad.id));
    tmp16.copy(pad.id, arraysize(pad.id) - 1);

    pad.connected = true;
  }
}

void GamepadPlatformDataFetcherLinux::EnumerateDevices() {
  udev_enumerate* enumerate = udev_enumerate_new(udev_);
  if (!enumerate)
    return;
  int ret = udev_enumerate_add_match_subsystem(enumerate, kInputSubsystem);
  if (ret != 0)
    return;
  ret = udev_enumerate_scan_devices(enumerate);
  if (ret != 0)
    return;

  udev_list_entry* devices = udev_enumerate_get_list_entry(enumerate);
  for (udev_list_entry* dev_list_entry = devices;
       dev_list_entry != NULL;
       dev_list_entry = udev_list_entry_get_next(dev_list_entry)) {
    // Get the filename of the /sys entry for the device and create a
    // udev_device object (dev) representing it
    const char* path = udev_list_entry_get_name(dev_list_entry);
    udev_device* dev = udev_device_new_from_syspath(udev_, path);
    if (!dev)
      continue;
    RefreshDevice(dev);
    udev_device_unref(dev);
  }
  // Free the enumerator object
  udev_enumerate_unref(enumerate);
}

void GamepadPlatformDataFetcherLinux::ReadDeviceData(size_t index) {
  // Linker does not like CHECK_LT(index, WebGamepads::itemsLengthCap). =/
  if (index >= WebGamepads::itemsLengthCap) {
    CHECK(false);
    return;
  }

  const int& fd = device_fds_[index];
  WebGamepad& pad = data_.items[index];
  DCHECK_GE(fd, 0);

  js_event event;
  while (HANDLE_EINTR(read(fd, &event, sizeof(struct js_event)) > 0)) {
    size_t item = event.number;
    if (event.type & JS_EVENT_AXIS) {
      if (item >= WebGamepad::axesLengthCap)
        continue;
      pad.axes[item] = event.value / kMaxLinuxAxisValue;
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
