// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_USB_MOJO_DEVICE_MANAGER_TEST_H_
#define DEVICE_USB_MOJO_DEVICE_MANAGER_TEST_H_

#include <string>

#include "base/macros.h"
#include "device/usb/public/mojom/device_manager_test.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"

namespace device {
namespace usb {

class DeviceManagerTest : public mojom::UsbDeviceManagerTest {
 public:
  DeviceManagerTest();
  ~DeviceManagerTest() override;

  void BindRequest(mojom::UsbDeviceManagerTestRequest request);

 private:
  // mojom::DeviceManagerTest overrides:
  void AddDeviceForTesting(const std::string& name,
                           const std::string& serial_number,
                           const std::string& landing_page,
                           AddDeviceForTestingCallback callback) override;
  void RemoveDeviceForTesting(const std::string& guid,
                              RemoveDeviceForTestingCallback callback) override;
  void GetTestDevices(GetTestDevicesCallback callback) override;

 private:
  mojo::BindingSet<mojom::UsbDeviceManagerTest> bindings_;

  DISALLOW_COPY_AND_ASSIGN(DeviceManagerTest);
};

}  // namespace usb
}  // namespace device

#endif  // DEVICE_USB_MOJO_DEVICE_MANAGER_TEST_H_
