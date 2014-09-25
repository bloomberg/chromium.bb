// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/memory/ref_counted.h"
#include "device/usb/usb_descriptors.h"
#include "device/usb/usb_device.h"
#include "device/usb/usb_device_filter.h"
#include "device/usb/usb_device_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

using testing::NiceMock;
using testing::ReturnRef;

class MockUsbDevice : public UsbDevice {
 public:
  MockUsbDevice(uint16 vendor_id, uint16 product_id, uint32 unique_id)
      : UsbDevice(vendor_id, product_id, unique_id) {}

  MOCK_METHOD0(Open, scoped_refptr<UsbDeviceHandle>());
  MOCK_METHOD1(Close, bool(scoped_refptr<UsbDeviceHandle>));
#if defined(OS_CHROMEOS)
  MOCK_METHOD2(RequestUsbAccess, void(int, const base::Callback<void(bool)>&));
#endif
  MOCK_METHOD0(GetConfiguration, const UsbConfigDescriptor&());
  MOCK_METHOD1(GetManufacturer, bool(base::string16*));
  MOCK_METHOD1(GetProduct, bool(base::string16*));
  MOCK_METHOD1(GetSerialNumber, bool(base::string16*));

 private:
  virtual ~MockUsbDevice() {}
};

class UsbFilterTest : public testing::Test {
 public:
  virtual void SetUp() OVERRIDE {
    UsbInterfaceDescriptor interface;
    interface.interface_number = 1;
    interface.alternate_setting = 0;
    interface.interface_class = 0xFF;
    interface.interface_subclass = 0x42;
    interface.interface_protocol = 0x01;
    config_.interfaces.push_back(interface);

    android_phone_ = new MockUsbDevice(0x18d1, 0x4ee2, 0);
    ON_CALL(*android_phone_.get(), GetConfiguration())
        .WillByDefault(ReturnRef(config_));
  }

 protected:
  UsbConfigDescriptor config_;
  scoped_refptr<MockUsbDevice> android_phone_;
};

TEST_F(UsbFilterTest, MatchAny) {
  UsbDeviceFilter filter;
  ASSERT_TRUE(filter.Matches(android_phone_));
}

TEST_F(UsbFilterTest, MatchVendorId) {
  UsbDeviceFilter filter;
  filter.SetVendorId(0x18d1);
  ASSERT_TRUE(filter.Matches(android_phone_));
}

TEST_F(UsbFilterTest, MatchVendorIdNegative) {
  UsbDeviceFilter filter;
  filter.SetVendorId(0x1d6b);
  ASSERT_FALSE(filter.Matches(android_phone_));
}

TEST_F(UsbFilterTest, MatchProductId) {
  UsbDeviceFilter filter;
  filter.SetVendorId(0x18d1);
  filter.SetProductId(0x4ee2);
  ASSERT_TRUE(filter.Matches(android_phone_));
}

TEST_F(UsbFilterTest, MatchProductIdNegative) {
  UsbDeviceFilter filter;
  filter.SetVendorId(0x18d1);
  filter.SetProductId(0x4ee1);
  ASSERT_FALSE(filter.Matches(android_phone_));
}

TEST_F(UsbFilterTest, MatchInterfaceClass) {
  UsbDeviceFilter filter;
  filter.SetInterfaceClass(0xff);
  ASSERT_TRUE(filter.Matches(android_phone_));
}

TEST_F(UsbFilterTest, MatchInterfaceClassNegative) {
  UsbDeviceFilter filter;
  filter.SetInterfaceClass(0xe0);
  ASSERT_FALSE(filter.Matches(android_phone_));
}

TEST_F(UsbFilterTest, MatchInterfaceSubclass) {
  UsbDeviceFilter filter;
  filter.SetInterfaceClass(0xff);
  filter.SetInterfaceSubclass(0x42);
  ASSERT_TRUE(filter.Matches(android_phone_));
}

TEST_F(UsbFilterTest, MatchInterfaceSubclassNegative) {
  UsbDeviceFilter filter;
  filter.SetInterfaceClass(0xff);
  filter.SetInterfaceSubclass(0x01);
  ASSERT_FALSE(filter.Matches(android_phone_));
}

TEST_F(UsbFilterTest, MatchInterfaceProtocol) {
  UsbDeviceFilter filter;
  filter.SetInterfaceClass(0xff);
  filter.SetInterfaceSubclass(0x42);
  filter.SetInterfaceProtocol(0x01);
  ASSERT_TRUE(filter.Matches(android_phone_));
}

TEST_F(UsbFilterTest, MatchInterfaceProtocolNegative) {
  UsbDeviceFilter filter;
  filter.SetInterfaceClass(0xff);
  filter.SetInterfaceSubclass(0x42);
  filter.SetInterfaceProtocol(0x02);
  ASSERT_FALSE(filter.Matches(android_phone_));
}

TEST_F(UsbFilterTest, MatchAnyEmptyListNegative) {
  std::vector<UsbDeviceFilter> filters;
  ASSERT_FALSE(UsbDeviceFilter::MatchesAny(android_phone_, filters));
}

TEST_F(UsbFilterTest, MatchesAnyVendorId) {
  std::vector<UsbDeviceFilter> filters(1);
  filters.back().SetVendorId(0x18d1);
  ASSERT_TRUE(UsbDeviceFilter::MatchesAny(android_phone_, filters));
}

TEST_F(UsbFilterTest, MatchesAnyVendorIdNegative) {
  std::vector<UsbDeviceFilter> filters(1);
  filters.back().SetVendorId(0x1d6b);
  ASSERT_FALSE(UsbDeviceFilter::MatchesAny(android_phone_, filters));
}

}  // namespace

}  // namespace device
