// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_service_record_mac.h"

#include <IOBluetooth/Bluetooth.h>
#import <IOBluetooth/IOBluetoothUtilities.h>
#import <IOBluetooth/objc/IOBluetoothDevice.h>
#import <IOBluetooth/objc/IOBluetoothSDPDataElement.h>
#import <IOBluetooth/objc/IOBluetoothSDPServiceRecord.h>
#import <IOBluetooth/objc/IOBluetoothSDPUUID.h>

#include <string>

#include "base/basictypes.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"

namespace {

void ExtractUuid(IOBluetoothSDPDataElement* service_class_data,
                 std::string* uuid) {
  NSArray* inner_elements = [service_class_data getArrayValue];
  IOBluetoothSDPUUID* sdp_uuid = nil;
  for (IOBluetoothSDPDataElement* inner_element in inner_elements) {
    if ([inner_element getTypeDescriptor] == kBluetoothSDPDataElementTypeUUID) {
      sdp_uuid = [[inner_element getUUIDValue] getUUIDWithLength:16];
      break;
    }
  }
  if (sdp_uuid != nil) {
    const uint8* uuid_bytes = reinterpret_cast<const uint8*>([sdp_uuid bytes]);
    *uuid = base::StringPrintf(
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        uuid_bytes[0], uuid_bytes[1], uuid_bytes[2], uuid_bytes[3],
        uuid_bytes[4], uuid_bytes[5], uuid_bytes[6], uuid_bytes[7],
        uuid_bytes[8], uuid_bytes[9], uuid_bytes[10], uuid_bytes[11],
        uuid_bytes[12], uuid_bytes[13], uuid_bytes[14], uuid_bytes[15]);
  }
}

}  // namespace

namespace device {

BluetoothServiceRecordMac::BluetoothServiceRecordMac(
    IOBluetoothSDPServiceRecord* record)
    : BluetoothServiceRecord() {
  name_ = base::SysNSStringToUTF8([record getServiceName]);
  device_ = [[record device] retain];
  address_ = base::SysNSStringToUTF8(IOBluetoothNSStringFromDeviceAddress(
      [device_ getAddress]));

  // TODO(youngki): Extract these values from |record|.
  supports_hid_ = false;
  hid_reconnect_initiate_ = false;
  hid_normally_connectable_ = false;

  supports_rfcomm_ =
      [record getRFCOMMChannelID:&rfcomm_channel_] == kIOReturnSuccess;

  const BluetoothSDPServiceAttributeID service_class_id = 1;

  IOBluetoothSDPDataElement* service_class_data =
      [record getAttributeDataElement:service_class_id];
  if ([service_class_data getTypeDescriptor] ==
          kBluetoothSDPDataElementTypeDataElementSequence) {
    ExtractUuid(service_class_data, &uuid_);
  }
}

BluetoothServiceRecordMac::~BluetoothServiceRecordMac() {
  [device_ release];
}

}  // namespace device
