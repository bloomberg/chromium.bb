// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/usb_descriptors.h"

#include <stdint.h>

#include <memory>

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "device/usb/mock_usb_device_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace device {

namespace {

ACTION_P2(InvokeCallback, data, length) {
  size_t transferred_length = std::min(length, arg7);
  memcpy(arg6->data(), data, transferred_length);
  arg9.Run(USB_TRANSFER_COMPLETED, arg6, transferred_length);
}

void ExpectStringDescriptors(
    std::unique_ptr<std::map<uint8_t, base::string16>> string_map) {
  EXPECT_EQ(3u, string_map->size());
  EXPECT_EQ(base::ASCIIToUTF16("String 1"), (*string_map)[1]);
  EXPECT_EQ(base::ASCIIToUTF16("String 2"), (*string_map)[2]);
  EXPECT_EQ(base::ASCIIToUTF16("String 3"), (*string_map)[3]);
}

class UsbDescriptorsTest : public ::testing::Test {};

TEST_F(UsbDescriptorsTest, ParseConfigs) {
  const std::vector<uint8_t> kConfigDescriptors{
      // Device
      0x12, 0x01, 0x10, 0x03, 0xFF, 0xFF, 0xFF, 0x09, 0x34, 0x12, 0x78, 0x56,
      0x00, 0x01, 0x01, 0x02, 0x03, 0x02,
      // Config 1
      0x09, 0x02, 0x38, 0x00, 0x02, 0x01, 0x01, 0x01, 0x10,
      // Interface Association (0 + 1)
      0x08, 0x0B, 0x00, 0x02, 0xFF, 0xFF, 0xFF, 0x00,
      // Interface 0
      0x09, 0x04, 0x00, 0x00, 0x03, 0x12, 0x34, 0x56, 0x02,
      // Endpoint 1 IN
      0x07, 0x05, 0x81, 0x02, 0x00, 0x02, 0x00,
      // Endpoint 2 IN
      0x07, 0x05, 0x82, 0x03, 0x00, 0x02, 0x04,
      // Endpoint 3 OUT
      0x07, 0x05, 0x03, 0x13, 0x00, 0x02, 0x04,
      // Interface 1
      0x09, 0x04, 0x01, 0x00, 0x00, 0x78, 0x9A, 0xAB, 0x03,
      // Config 2
      0x09, 0x02, 0x29, 0x00, 0x01, 0x02, 0x04, 0x03, 0x20,
      // Interface 0
      0x09, 0x04, 0x00, 0x00, 0x00, 0xCD, 0xEF, 0x01, 0x04,
      // Interface 0 (alternate 1)
      0x09, 0x04, 0x00, 0x01, 0x02, 0xCD, 0xEF, 0x01, 0x05,
      // Endpoint 1 IN
      0x07, 0x05, 0x81, 0x01, 0x00, 0x04, 0x08,
      // Endpoint 2 OUT
      0x07, 0x05, 0x02, 0x11, 0x00, 0x04, 0x08,
  };

  UsbDeviceDescriptor desc;
  ASSERT_TRUE(desc.Parse(kConfigDescriptors));

  // Device
  EXPECT_EQ(0x0310, desc.usb_version);
  EXPECT_EQ(0xFF, desc.device_class);
  EXPECT_EQ(0xFF, desc.device_subclass);
  EXPECT_EQ(0xFF, desc.device_protocol);
  EXPECT_EQ(0x1234, desc.vendor_id);
  EXPECT_EQ(0x5678, desc.product_id);
  EXPECT_EQ(0x0100, desc.device_version);
  ASSERT_EQ(2u, desc.configurations.size());
  // Config 1
  EXPECT_EQ(1, desc.configurations[0].configuration_value);
  EXPECT_FALSE(desc.configurations[0].self_powered);
  EXPECT_FALSE(desc.configurations[0].remote_wakeup);
  EXPECT_EQ(16, desc.configurations[0].maximum_power);
  ASSERT_EQ(2u, desc.configurations[0].interfaces.size());
  EXPECT_EQ(8u, desc.configurations[0].extra_data.size());
  // Interface 0
  EXPECT_EQ(0, desc.configurations[0].interfaces[0].interface_number);
  EXPECT_EQ(0, desc.configurations[0].interfaces[0].alternate_setting);
  EXPECT_EQ(0x12, desc.configurations[0].interfaces[0].interface_class);
  EXPECT_EQ(0x34, desc.configurations[0].interfaces[0].interface_subclass);
  EXPECT_EQ(0x56, desc.configurations[0].interfaces[0].interface_protocol);
  ASSERT_EQ(3u, desc.configurations[0].interfaces[0].endpoints.size());
  EXPECT_EQ(0u, desc.configurations[0].interfaces[0].extra_data.size());
  EXPECT_EQ(0, desc.configurations[0].interfaces[0].first_interface);
  // Endpoint 1 IN
  EXPECT_EQ(0x81, desc.configurations[0].interfaces[0].endpoints[0].address);
  EXPECT_EQ(USB_DIRECTION_INBOUND,
            desc.configurations[0].interfaces[0].endpoints[0].direction);
  EXPECT_EQ(
      512,
      desc.configurations[0].interfaces[0].endpoints[0].maximum_packet_size);
  EXPECT_EQ(
      USB_SYNCHRONIZATION_NONE,
      desc.configurations[0].interfaces[0].endpoints[0].synchronization_type);
  EXPECT_EQ(USB_TRANSFER_BULK,
            desc.configurations[0].interfaces[0].endpoints[0].transfer_type);
  EXPECT_EQ(USB_USAGE_RESERVED,
            desc.configurations[0].interfaces[0].endpoints[0].usage_type);
  EXPECT_EQ(0,
            desc.configurations[0].interfaces[0].endpoints[0].polling_interval);
  EXPECT_EQ(
      0u, desc.configurations[0].interfaces[0].endpoints[0].extra_data.size());
  // Endpoint 2 IN
  EXPECT_EQ(0x82, desc.configurations[0].interfaces[0].endpoints[1].address);
  EXPECT_EQ(USB_DIRECTION_INBOUND,
            desc.configurations[0].interfaces[0].endpoints[1].direction);
  EXPECT_EQ(
      512,
      desc.configurations[0].interfaces[0].endpoints[1].maximum_packet_size);
  EXPECT_EQ(
      USB_SYNCHRONIZATION_NONE,
      desc.configurations[0].interfaces[0].endpoints[1].synchronization_type);
  EXPECT_EQ(USB_TRANSFER_INTERRUPT,
            desc.configurations[0].interfaces[0].endpoints[1].transfer_type);
  EXPECT_EQ(USB_USAGE_PERIODIC,
            desc.configurations[0].interfaces[0].endpoints[1].usage_type);
  EXPECT_EQ(4,
            desc.configurations[0].interfaces[0].endpoints[1].polling_interval);
  EXPECT_EQ(
      0u, desc.configurations[0].interfaces[0].endpoints[1].extra_data.size());
  // Endpoint 3 OUT
  EXPECT_EQ(0x03, desc.configurations[0].interfaces[0].endpoints[2].address);
  EXPECT_EQ(USB_DIRECTION_OUTBOUND,
            desc.configurations[0].interfaces[0].endpoints[2].direction);
  EXPECT_EQ(
      512,
      desc.configurations[0].interfaces[0].endpoints[2].maximum_packet_size);
  EXPECT_EQ(
      USB_SYNCHRONIZATION_NONE,
      desc.configurations[0].interfaces[0].endpoints[2].synchronization_type);
  EXPECT_EQ(USB_TRANSFER_INTERRUPT,
            desc.configurations[0].interfaces[0].endpoints[2].transfer_type);
  EXPECT_EQ(USB_USAGE_NOTIFICATION,
            desc.configurations[0].interfaces[0].endpoints[2].usage_type);
  EXPECT_EQ(4,
            desc.configurations[0].interfaces[0].endpoints[2].polling_interval);
  EXPECT_EQ(
      0u, desc.configurations[0].interfaces[0].endpoints[2].extra_data.size());
  // Interface 1
  EXPECT_EQ(1, desc.configurations[0].interfaces[1].interface_number);
  EXPECT_EQ(0, desc.configurations[0].interfaces[1].alternate_setting);
  EXPECT_EQ(0x78, desc.configurations[0].interfaces[1].interface_class);
  EXPECT_EQ(0x9A, desc.configurations[0].interfaces[1].interface_subclass);
  EXPECT_EQ(0xAB, desc.configurations[0].interfaces[1].interface_protocol);
  ASSERT_EQ(0u, desc.configurations[0].interfaces[1].endpoints.size());
  EXPECT_EQ(0u, desc.configurations[0].interfaces[1].extra_data.size());
  EXPECT_EQ(0, desc.configurations[0].interfaces[1].first_interface);
  // Config 2
  EXPECT_EQ(2, desc.configurations[1].configuration_value);
  EXPECT_TRUE(desc.configurations[1].self_powered);
  EXPECT_FALSE(desc.configurations[1].remote_wakeup);
  EXPECT_EQ(32, desc.configurations[1].maximum_power);
  ASSERT_EQ(2u, desc.configurations[1].interfaces.size());
  EXPECT_EQ(0u, desc.configurations[1].extra_data.size());
  // Interface 0
  EXPECT_EQ(0, desc.configurations[1].interfaces[0].interface_number);
  EXPECT_EQ(0, desc.configurations[1].interfaces[0].alternate_setting);
  EXPECT_EQ(0xCD, desc.configurations[1].interfaces[0].interface_class);
  EXPECT_EQ(0xEF, desc.configurations[1].interfaces[0].interface_subclass);
  EXPECT_EQ(0x01, desc.configurations[1].interfaces[0].interface_protocol);
  ASSERT_EQ(0u, desc.configurations[1].interfaces[0].endpoints.size());
  EXPECT_EQ(0u, desc.configurations[1].interfaces[0].extra_data.size());
  EXPECT_EQ(0, desc.configurations[1].interfaces[0].first_interface);
  // Interface 0 (alternate 1)
  EXPECT_EQ(0, desc.configurations[1].interfaces[1].interface_number);
  EXPECT_EQ(1, desc.configurations[1].interfaces[1].alternate_setting);
  EXPECT_EQ(0xCD, desc.configurations[1].interfaces[1].interface_class);
  EXPECT_EQ(0xEF, desc.configurations[1].interfaces[1].interface_subclass);
  EXPECT_EQ(0x01, desc.configurations[1].interfaces[1].interface_protocol);
  ASSERT_EQ(2u, desc.configurations[1].interfaces[1].endpoints.size());
  EXPECT_EQ(0u, desc.configurations[1].interfaces[1].extra_data.size());
  EXPECT_EQ(0, desc.configurations[1].interfaces[1].first_interface);
  // Endpoint 1 IN
  EXPECT_EQ(0x81, desc.configurations[1].interfaces[1].endpoints[0].address);
  EXPECT_EQ(USB_DIRECTION_INBOUND,
            desc.configurations[1].interfaces[1].endpoints[0].direction);
  EXPECT_EQ(
      1024,
      desc.configurations[1].interfaces[1].endpoints[0].maximum_packet_size);
  EXPECT_EQ(
      USB_SYNCHRONIZATION_NONE,
      desc.configurations[1].interfaces[1].endpoints[0].synchronization_type);
  EXPECT_EQ(USB_TRANSFER_ISOCHRONOUS,
            desc.configurations[1].interfaces[1].endpoints[0].transfer_type);
  EXPECT_EQ(USB_USAGE_DATA,
            desc.configurations[1].interfaces[1].endpoints[0].usage_type);
  EXPECT_EQ(8,
            desc.configurations[1].interfaces[1].endpoints[0].polling_interval);
  EXPECT_EQ(
      0u, desc.configurations[1].interfaces[1].endpoints[0].extra_data.size());
  // Endpoint 2 OUT
  EXPECT_EQ(0x02, desc.configurations[1].interfaces[1].endpoints[1].address);
  EXPECT_EQ(USB_DIRECTION_OUTBOUND,
            desc.configurations[1].interfaces[1].endpoints[1].direction);
  EXPECT_EQ(
      1024,
      desc.configurations[1].interfaces[1].endpoints[1].maximum_packet_size);
  EXPECT_EQ(
      USB_SYNCHRONIZATION_NONE,
      desc.configurations[1].interfaces[1].endpoints[1].synchronization_type);
  EXPECT_EQ(USB_TRANSFER_ISOCHRONOUS,
            desc.configurations[1].interfaces[1].endpoints[1].transfer_type);
  EXPECT_EQ(USB_USAGE_FEEDBACK,
            desc.configurations[1].interfaces[1].endpoints[1].usage_type);
  EXPECT_EQ(8,
            desc.configurations[1].interfaces[1].endpoints[1].polling_interval);
  EXPECT_EQ(
      0u, desc.configurations[1].interfaces[1].endpoints[1].extra_data.size());
}

TEST_F(UsbDescriptorsTest, NoInterfaceAssociations) {
  UsbConfigDescriptor config(1, false, false, 0);
  config.interfaces.emplace_back(0, 0, 255, 255, 255);
  config.interfaces.emplace_back(0, 1, 255, 255, 255);
  config.interfaces.emplace_back(1, 0, 255, 255, 255);
  config.AssignFirstInterfaceNumbers();

  EXPECT_EQ(0, config.interfaces[0].first_interface);
  EXPECT_EQ(0, config.interfaces[1].first_interface);
  EXPECT_EQ(1, config.interfaces[2].first_interface);
}

TEST_F(UsbDescriptorsTest, InterfaceAssociations) {
  // Links interfaces 0 and 1 into a single function.
  static const uint8_t kIAD1[] = {0x08, 0x0b, 0x00, 0x02,
                                  0xff, 0xff, 0xff, 0x00};
  // Only references a single interface, 2.
  static const uint8_t kIAD2[] = {0x08, 0x0b, 0x02, 0x01,
                                  0xff, 0xff, 0xff, 0x00};
  // Malformed. References interface 3 but bInterfaceCount is 0.
  static const uint8_t kIAD3[] = {0x08, 0x0b, 0x03, 0x00,
                                  0xff, 0xff, 0xff, 0x00};
  // Links interfaces 4 and 5 into a single function.
  static const uint8_t kIAD4[] = {0x08, 0x0b, 0x04, 0x02,
                                  0xff, 0xff, 0xff, 0x00};

  UsbConfigDescriptor config(1, false, false, 0);
  config.extra_data.assign(kIAD1, kIAD1 + sizeof(kIAD1));
  config.extra_data.insert(config.extra_data.end(), kIAD2,
                           kIAD2 + sizeof(kIAD2));
  config.interfaces.emplace_back(0, 0, 255, 255, 255);
  config.interfaces.emplace_back(1, 0, 255, 255, 255);
  UsbInterfaceDescriptor iface1a(1, 1, 255, 255, 255);
  iface1a.extra_data.assign(kIAD3, kIAD3 + sizeof(kIAD3));
  config.interfaces.push_back(std::move(iface1a));
  config.interfaces.emplace_back(2, 0, 255, 255, 255);
  config.interfaces.emplace_back(3, 0, 255, 255, 255);
  UsbInterfaceDescriptor iface4(4, 0, 255, 255, 255);
  iface4.extra_data.assign(kIAD4, kIAD4 + sizeof(kIAD4));
  config.interfaces.push_back(std::move(iface4));
  config.interfaces.emplace_back(5, 0, 255, 255, 255);
  config.AssignFirstInterfaceNumbers();

  // Interfaces 0 and 1 (plus 1's alternate) are a single function.
  EXPECT_EQ(0, config.interfaces[0].interface_number);
  EXPECT_EQ(0, config.interfaces[0].first_interface);
  EXPECT_EQ(1, config.interfaces[1].interface_number);
  EXPECT_EQ(0, config.interfaces[1].first_interface);
  EXPECT_EQ(1, config.interfaces[2].interface_number);
  EXPECT_EQ(0, config.interfaces[2].first_interface);

  // Interfaces 2 and 3 are their own functions.
  EXPECT_EQ(2, config.interfaces[3].interface_number);
  EXPECT_EQ(2, config.interfaces[3].first_interface);
  EXPECT_EQ(3, config.interfaces[4].interface_number);
  EXPECT_EQ(3, config.interfaces[4].first_interface);

  // Interfaces 4 and 5 are a single function.
  EXPECT_EQ(4, config.interfaces[5].interface_number);
  EXPECT_EQ(4, config.interfaces[5].first_interface);
  EXPECT_EQ(5, config.interfaces[6].interface_number);
  EXPECT_EQ(4, config.interfaces[6].first_interface);
}

TEST_F(UsbDescriptorsTest, CorruptInterfaceAssociations) {
  {
    // Descriptor is too short.
    static const uint8_t kIAD[] = {0x01};
    UsbConfigDescriptor config(1, false, false, 0);
    config.extra_data.assign(kIAD, kIAD + sizeof(kIAD));
    config.AssignFirstInterfaceNumbers();
  }
  {
    // Descriptor is too long.
    static const uint8_t kIAD[] = {0x09, 0x0b, 0x00, 0x00,
                                   0x00, 0x00, 0x00, 0x00};
    UsbConfigDescriptor config(1, false, false, 0);
    config.extra_data.assign(kIAD, kIAD + sizeof(kIAD));
    config.AssignFirstInterfaceNumbers();
  }
  {
    // References an undefined interface.
    static const uint8_t kIAD[] = {0x08, 0x0b, 0x07, 0x00,
                                   0xff, 0xff, 0xff, 0x00};
    UsbConfigDescriptor config(1, false, false, 0);
    config.interfaces.emplace_back(0, 0, 255, 255, 255);
    config.extra_data.assign(kIAD, kIAD + sizeof(kIAD));
    config.AssignFirstInterfaceNumbers();

    EXPECT_EQ(0, config.interfaces[0].interface_number);
    EXPECT_EQ(0, config.interfaces[0].first_interface);
  }
}

TEST_F(UsbDescriptorsTest, StringDescriptor) {
  static const uint8_t kBuffer[] = {0x1a, 0x03, 'H', 0, 'e', 0, 'l', 0, 'l', 0,
                                    'o',  0,    ' ', 0, 'w', 0, 'o', 0, 'r', 0,
                                    'l',  0,    'd', 0, '!', 0};
  base::string16 string;
  ASSERT_TRUE(ParseUsbStringDescriptor(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer)), &string));
  EXPECT_EQ(base::ASCIIToUTF16("Hello world!"), string);
}

TEST_F(UsbDescriptorsTest, ShortStringDescriptorHeader) {
  // The buffer is just too darn short.
  static const uint8_t kBuffer[] = {0x01};
  base::string16 string;
  ASSERT_FALSE(ParseUsbStringDescriptor(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer)), &string));
}

TEST_F(UsbDescriptorsTest, ShortStringDescriptor) {
  // The buffer is just too darn short.
  static const uint8_t kBuffer[] = {0x01, 0x03};
  base::string16 string;
  ASSERT_FALSE(ParseUsbStringDescriptor(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer)), &string));
}

TEST_F(UsbDescriptorsTest, OddLengthStringDescriptor) {
  // There's an extra byte at the end of the string.
  static const uint8_t kBuffer[] = {0x0d, 0x03, 'H', 0,   'e', 0,  'l',
                                    0,    'l',  0,   'o', 0,   '!'};
  base::string16 string;
  ASSERT_TRUE(ParseUsbStringDescriptor(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer)), &string));
  EXPECT_EQ(base::ASCIIToUTF16("Hello"), string);
}

TEST_F(UsbDescriptorsTest, EmptyStringDescriptor) {
  // The string is empty.
  static const uint8_t kBuffer[] = {0x02, 0x03};
  base::string16 string;
  ASSERT_TRUE(ParseUsbStringDescriptor(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer)), &string));
  EXPECT_EQ(base::string16(), string);
}

TEST_F(UsbDescriptorsTest, OneByteStringDescriptor) {
  // The string is only one byte.
  static const uint8_t kBuffer[] = {0x03, 0x03, '?'};
  base::string16 string;
  ASSERT_TRUE(ParseUsbStringDescriptor(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer)), &string));
  EXPECT_EQ(base::string16(), string);
}

TEST_F(UsbDescriptorsTest, ReadStringDescriptors) {
  std::unique_ptr<std::map<uint8_t, base::string16>> string_map(
      new std::map<uint8_t, base::string16>());
  (*string_map)[1] = base::string16();
  (*string_map)[2] = base::string16();
  (*string_map)[3] = base::string16();

  scoped_refptr<MockUsbDeviceHandle> device_handle(
      new MockUsbDeviceHandle(nullptr));
  static const uint8_t kStringDescriptor0[] = {0x04, 0x03, 0x21, 0x43};
  EXPECT_CALL(*device_handle,
              ControlTransfer(USB_DIRECTION_INBOUND, UsbDeviceHandle::STANDARD,
                              UsbDeviceHandle::DEVICE, 0x06, 0x0300, 0x0000, _,
                              _, _, _))
      .WillOnce(InvokeCallback(kStringDescriptor0, sizeof(kStringDescriptor0)));
  static const uint8_t kStringDescriptor1[] = {0x12, 0x03, 'S', 0, 't', 0,
                                               'r',  0,    'i', 0, 'n', 0,
                                               'g',  0,    ' ', 0, '1', 0};
  EXPECT_CALL(*device_handle,
              ControlTransfer(USB_DIRECTION_INBOUND, UsbDeviceHandle::STANDARD,
                              UsbDeviceHandle::DEVICE, 0x06, 0x0301, 0x4321, _,
                              _, _, _))
      .WillOnce(InvokeCallback(kStringDescriptor1, sizeof(kStringDescriptor1)));
  static const uint8_t kStringDescriptor2[] = {0x12, 0x03, 'S', 0, 't', 0,
                                               'r',  0,    'i', 0, 'n', 0,
                                               'g',  0,    ' ', 0, '2', 0};
  EXPECT_CALL(*device_handle,
              ControlTransfer(USB_DIRECTION_INBOUND, UsbDeviceHandle::STANDARD,
                              UsbDeviceHandle::DEVICE, 0x06, 0x0302, 0x4321, _,
                              _, _, _))
      .WillOnce(InvokeCallback(kStringDescriptor2, sizeof(kStringDescriptor2)));
  static const uint8_t kStringDescriptor3[] = {0x12, 0x03, 'S', 0, 't', 0,
                                               'r',  0,    'i', 0, 'n', 0,
                                               'g',  0,    ' ', 0, '3', 0};
  EXPECT_CALL(*device_handle,
              ControlTransfer(USB_DIRECTION_INBOUND, UsbDeviceHandle::STANDARD,
                              UsbDeviceHandle::DEVICE, 0x06, 0x0303, 0x4321, _,
                              _, _, _))
      .WillOnce(InvokeCallback(kStringDescriptor3, sizeof(kStringDescriptor3)));

  ReadUsbStringDescriptors(device_handle, std::move(string_map),
                           base::Bind(&ExpectStringDescriptors));
}

}  // namespace

}  // namespace device
