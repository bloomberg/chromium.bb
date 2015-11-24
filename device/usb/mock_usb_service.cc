// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/mock_usb_service.h"

#include <string>
#include <vector>

#include "device/usb/usb_device.h"

namespace device {

MockUsbService::MockUsbService() {
}

MockUsbService::~MockUsbService() {
}

void MockUsbService::AddDevice(scoped_refptr<UsbDevice> device) {
  devices_[device->guid()] = device;
  NotifyDeviceAdded(device);
}

void MockUsbService::RemoveDevice(scoped_refptr<UsbDevice> device) {
  devices_.erase(device->guid());
  UsbService::NotifyDeviceRemoved(device);
}

scoped_refptr<UsbDevice> MockUsbService::GetDevice(const std::string& guid) {
  auto it = devices_.find(guid);
  if (it != devices_.end())
    return it->second;
  return nullptr;
}

void MockUsbService::GetDevices(const GetDevicesCallback& callback) {
  std::vector<scoped_refptr<UsbDevice>> devices;
  for (const auto& map_entry : devices_)
    devices.push_back(map_entry.second);
  callback.Run(devices);
}

}  // namespace device
