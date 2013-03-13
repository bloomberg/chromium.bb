// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_service_record_mac.h"

#import <IOBluetooth/objc/IOBluetoothSDPDataElement.h>
#import <IOBluetooth/objc/IOBluetoothSDPServiceRecord.h>
#import <IOBluetooth/objc/IOBluetoothSDPUUID.h>

#include <string>

#include "base/basictypes.h"
#include "base/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const BluetoothSDPServiceAttributeID kServiceClassIDAttributeId = 0x0001;
const BluetoothSDPServiceAttributeID kProtocolDescriptorListAttributeId =
    0x0004;
const BluetoothSDPServiceAttributeID kServiceNameAttributeId = 0x0100;

const uint8 kRfcommChannel = 0x0c;
const char kServiceName[] = "Headset Audio Gateway";

const char kExpectedRfcommUuid[] = "01234567-89ab-cdef-0123-456789abcdef";
const char kExpectedSerialUuid[] = "00001101-0000-1000-8000-00805f9b34fb";

const int kMaxUuidSize = 16;

}  // namespace

namespace device {

class BluetoothServiceRecordMacTest : public testing::Test {
 public:

  IOBluetoothSDPUUID* ConvertUuid(const char* uuid_hex_char, size_t uuid_size) {
    std::vector<uint8> uuid_bytes_vector;
    uint8 uuid_buffer[kMaxUuidSize];
    base::HexStringToBytes(uuid_hex_char, &uuid_bytes_vector);
    std::copy(uuid_bytes_vector.begin(),
              uuid_bytes_vector.end(),
              uuid_buffer);
    return [IOBluetoothSDPUUID uuidWithBytes:uuid_buffer length:uuid_size];
  }

  IOBluetoothSDPDataElement* GetServiceClassId(IOBluetoothSDPUUID* uuid) {
    IOBluetoothSDPDataElement* uuid_element =
        [IOBluetoothSDPDataElement withElementValue:uuid];
    return [IOBluetoothSDPDataElement
        withElementValue:[NSArray arrayWithObject:uuid_element]];
  }

  IOBluetoothSDPDataElement* GetProtocolDescriptorList(bool supports_rfcomm) {
    NSMutableArray* protocol_descriptor_list_sequence = [NSMutableArray array];

    const uint8 l2cap_uuid_bytes[] = { 0x01, 0x00 };

    IOBluetoothSDPUUID* l2cap_uuid =
        [IOBluetoothSDPUUID uuidWithBytes:l2cap_uuid_bytes length:2];
    [protocol_descriptor_list_sequence
        addObject:[NSArray arrayWithObject:l2cap_uuid]];

    if (supports_rfcomm) {
      const uint8 rfcomm_uuid_bytes[] = { 0x00, 0x03 };
      IOBluetoothSDPUUID* rfcomm_uuid =
          [IOBluetoothSDPUUID uuidWithBytes:rfcomm_uuid_bytes length:2];
      NSNumber* rfcomm_channel =
          [NSNumber numberWithUnsignedChar:kRfcommChannel];
      [protocol_descriptor_list_sequence
          addObject:[NSArray
              arrayWithObjects:rfcomm_uuid, rfcomm_channel, nil]];
    }

    return [IOBluetoothSDPDataElement
        withElementValue:protocol_descriptor_list_sequence];
  }

  IOBluetoothSDPDataElement* GetServiceName(const std::string& service_name) {
    return [IOBluetoothSDPDataElement
        withElementValue:base::SysUTF8ToNSString(service_name)];
  }

  IOBluetoothSDPServiceRecord* GetServiceRecord(
      IOBluetoothSDPUUID* uuid, bool supports_rfcomm) {
    NSMutableDictionary* service_attrs = [NSMutableDictionary dictionary];

    if (uuid != nil) {
      [service_attrs
          setObject:GetServiceClassId(uuid)
             forKey:[NSNumber numberWithInt:kServiceClassIDAttributeId]];
    }
    [service_attrs
        setObject:GetProtocolDescriptorList(supports_rfcomm)
           forKey:[NSNumber numberWithInt:kProtocolDescriptorListAttributeId]];
    [service_attrs
        setObject:GetServiceName(kServiceName)
           forKey:[NSNumber numberWithInt:kServiceNameAttributeId]];

    return [IOBluetoothSDPServiceRecord withServiceDictionary:service_attrs
                                                       device:nil];
  }
};

TEST_F(BluetoothServiceRecordMacTest, RfcommService) {
  const char rfcomm_uuid_bytes[] = "0123456789abcdef0123456789abcdef";
  IOBluetoothSDPUUID* rfcomm_uuid =
      ConvertUuid(rfcomm_uuid_bytes, sizeof(rfcomm_uuid_bytes) / 2);

  BluetoothServiceRecordMac record(GetServiceRecord(rfcomm_uuid, true));
  EXPECT_EQ(kServiceName, record.name());
  EXPECT_TRUE(record.SupportsRfcomm());
  EXPECT_EQ(kRfcommChannel, record.rfcomm_channel());
  EXPECT_EQ(kExpectedRfcommUuid, record.uuid());
}

TEST_F(BluetoothServiceRecordMacTest, ShortUuid) {
  const char short_uuid_bytes[] = "1101";
  IOBluetoothSDPUUID* short_uuid =
      ConvertUuid(short_uuid_bytes, sizeof(short_uuid_bytes) / 2);

  BluetoothServiceRecordMac record(GetServiceRecord(short_uuid, false));
  EXPECT_EQ(kExpectedSerialUuid, record.uuid());
}

TEST_F(BluetoothServiceRecordMacTest, MediumUuid) {
  const char medium_uuid_bytes[] = "00001101";
  IOBluetoothSDPUUID* medium_uuid =
      ConvertUuid(medium_uuid_bytes, sizeof(medium_uuid_bytes) / 2);

  BluetoothServiceRecordMac record(GetServiceRecord(medium_uuid, false));
  EXPECT_EQ(kExpectedSerialUuid, record.uuid());
}

TEST_F(BluetoothServiceRecordMacTest, UpperCaseUuid) {
  const char upper_case_uuid_bytes[] = "0123456789ABCDEF0123456789ABCDEF";
  IOBluetoothSDPUUID* upper_case_uuid =
      ConvertUuid(upper_case_uuid_bytes, sizeof(upper_case_uuid_bytes) / 2);

  BluetoothServiceRecordMac record(GetServiceRecord(upper_case_uuid, false));
  EXPECT_EQ(kExpectedRfcommUuid, record.uuid());
}

TEST_F(BluetoothServiceRecordMacTest, InvalidUuid) {
  BluetoothServiceRecordMac record(GetServiceRecord(nil, false));
  EXPECT_EQ("", record.uuid());
}

}  // namespace device
