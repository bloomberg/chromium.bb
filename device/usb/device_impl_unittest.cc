// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "device/usb/device_impl.h"
#include "device/usb/mock_usb_device.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/interface_request.h"

namespace device {
namespace usb {

namespace {

using DeviceImplTest = testing::Test;

void ExpectDeviceInfoAndThen(const std::string& guid,
                             uint16_t vendor_id,
                             uint16_t product_id,
                             const std::string& manufacturer,
                             const std::string& product,
                             const std::string& serial_number,
                             const base::Closure& continuation,
                             DeviceInfoPtr device_info) {
  EXPECT_EQ(guid, device_info->guid);
  EXPECT_EQ(vendor_id, device_info->vendor_id);
  EXPECT_EQ(product_id, device_info->product_id);
  EXPECT_EQ(manufacturer, device_info->manufacturer);
  EXPECT_EQ(product, device_info->product);
  EXPECT_EQ(serial_number, device_info->serial_number);
  continuation.Run();
}

}  // namespace

// Test that the information returned via the Device::GetDeviceInfo matches that
// of the underlying device.
TEST_F(DeviceImplTest, GetDeviceInfo) {
  base::MessageLoop message_loop;
  scoped_refptr<MockUsbDevice> fake_device =
      new MockUsbDevice(0x1234, 0x5678, "ACME", "Frobinator", "ABCDEF");
  DevicePtr device;
  EXPECT_CALL(*fake_device.get(), GetConfiguration());
  new DeviceImpl(fake_device, mojo::GetProxy(&device));

  base::RunLoop run_loop;
  device->GetDeviceInfo(
      base::Bind(&ExpectDeviceInfoAndThen, fake_device->guid(), 0x1234, 0x5678,
                 "ACME", "Frobinator", "ABCDEF", run_loop.QuitClosure()));
  run_loop.Run();
}

}  // namespace usb
}  // namespace device
