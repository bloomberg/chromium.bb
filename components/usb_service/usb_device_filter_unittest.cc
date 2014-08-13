// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/memory/ref_counted.h"
#include "components/usb_service/usb_device.h"
#include "components/usb_service/usb_device_filter.h"
#include "components/usb_service/usb_device_handle.h"
#include "components/usb_service/usb_interface.h"
#include "testing/gtest/include/gtest/gtest.h"

using usb_service::UsbConfigDescriptor;
using usb_service::UsbDevice;
using usb_service::UsbDeviceFilter;
using usb_service::UsbDeviceHandle;
using usb_service::UsbEndpointDescriptor;
using usb_service::UsbInterfaceAltSettingDescriptor;
using usb_service::UsbInterfaceDescriptor;

namespace {

class MockUsbInterfaceAltSettingDescriptor
    : public UsbInterfaceAltSettingDescriptor {
 public:
  MockUsbInterfaceAltSettingDescriptor(int interface_number,
                                       int alternate_setting,
                                       int interface_class,
                                       int interface_subclass,
                                       int interface_protocol)
      : interface_number_(interface_number),
        alternate_setting_(alternate_setting),
        interface_class_(interface_class),
        interface_subclass_(interface_subclass),
        interface_protocol_(interface_protocol) {}

  virtual size_t GetNumEndpoints() const OVERRIDE { return 0; }
  virtual scoped_refptr<const UsbEndpointDescriptor> GetEndpoint(
      size_t index) const OVERRIDE {
    return NULL;
  }
  virtual int GetInterfaceNumber() const OVERRIDE { return interface_number_; }
  virtual int GetAlternateSetting() const OVERRIDE {
    return alternate_setting_;
  }
  virtual int GetInterfaceClass() const OVERRIDE { return interface_class_; }
  virtual int GetInterfaceSubclass() const OVERRIDE {
    return interface_subclass_;
  }
  virtual int GetInterfaceProtocol() const OVERRIDE {
    return interface_protocol_;
  }

 protected:
  virtual ~MockUsbInterfaceAltSettingDescriptor() {}

 private:
  int interface_number_;
  int alternate_setting_;
  int interface_class_;
  int interface_subclass_;
  int interface_protocol_;
};

typedef std::vector<scoped_refptr<UsbInterfaceAltSettingDescriptor> >
    UsbInterfaceAltSettingDescriptorList;

class MockUsbInterfaceDescriptor : public UsbInterfaceDescriptor {
 public:
  MockUsbInterfaceDescriptor(
      const UsbInterfaceAltSettingDescriptorList& alt_settings)
      : alt_settings_(alt_settings) {}

  virtual size_t GetNumAltSettings() const OVERRIDE {
    return alt_settings_.size();
  }
  virtual scoped_refptr<const UsbInterfaceAltSettingDescriptor> GetAltSetting(
      size_t index) const OVERRIDE {
    return alt_settings_[index];
  }

 protected:
  virtual ~MockUsbInterfaceDescriptor() {}

 private:
  UsbInterfaceAltSettingDescriptorList alt_settings_;
};

typedef std::vector<scoped_refptr<UsbInterfaceDescriptor> >
    UsbInterfaceDescriptorList;

class MockUsbConfigDescriptor : public UsbConfigDescriptor {
 public:
  MockUsbConfigDescriptor(const UsbInterfaceDescriptorList& interfaces)
      : interfaces_(interfaces) {}

  virtual size_t GetNumInterfaces() const OVERRIDE {
    return interfaces_.size();
  }
  virtual scoped_refptr<const UsbInterfaceDescriptor> GetInterface(
      size_t index) const OVERRIDE {
    return interfaces_[index];
  }

 protected:
  virtual ~MockUsbConfigDescriptor() {}

 private:
  UsbInterfaceDescriptorList interfaces_;
};

class MockUsbDevice : public UsbDevice {
 public:
  MockUsbDevice(uint16 vendor_id,
                uint16 product_id,
                uint32 unique_id,
                scoped_refptr<UsbConfigDescriptor> config_desc)
      : UsbDevice(vendor_id, product_id, unique_id),
        config_desc_(config_desc) {}

  virtual scoped_refptr<UsbDeviceHandle> Open() OVERRIDE { return NULL; }
  virtual bool Close(scoped_refptr<UsbDeviceHandle> handle) OVERRIDE {
    NOTREACHED();
    return true;
  }
#if defined(OS_CHROMEOS)
  virtual void RequestUsbAccess(
      int interface_id,
      const base::Callback<void(bool success)>& callback) OVERRIDE {
    NOTREACHED();
  }
#endif  // OS_CHROMEOS
  virtual scoped_refptr<UsbConfigDescriptor> ListInterfaces() OVERRIDE {
    return config_desc_;
  }

 protected:
  virtual ~MockUsbDevice() {}

 private:
  scoped_refptr<UsbConfigDescriptor> config_desc_;
};

class UsbFilterTest : public testing::Test {
 public:
  virtual void SetUp() OVERRIDE {
    UsbInterfaceAltSettingDescriptorList alt_settings;
    alt_settings.push_back(make_scoped_refptr(
        new MockUsbInterfaceAltSettingDescriptor(1, 0, 0xFF, 0x42, 1)));

    UsbInterfaceDescriptorList interfaces;
    interfaces.push_back(
        make_scoped_refptr(new MockUsbInterfaceDescriptor(alt_settings)));

    scoped_refptr<UsbConfigDescriptor> config_desc(
        new MockUsbConfigDescriptor(interfaces));

    android_phone_ = new MockUsbDevice(0x18d1, 0x4ee2, 0, config_desc);
  }

 protected:
  scoped_refptr<UsbDevice> android_phone_;
};

}  // namespace

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
