// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_profile_mac.h"

#import <IOBluetooth/objc/IOBluetoothDevice.h>
#import <IOBluetooth/objc/IOBluetoothSDPServiceRecord.h>
#import <IOBluetooth/objc/IOBluetoothSDPUUID.h>

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_number_conversions.h"
#include "device/bluetooth/bluetooth_device_mac.h"
#include "device/bluetooth/bluetooth_socket_mac.h"

namespace {

// Converts |uuid| to a IOBluetoothSDPUUID instance.
//
// |uuid| must be in the format of XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX.
IOBluetoothSDPUUID* GetIOBluetoothSDPUUID(const std::string& uuid) {
  DCHECK(uuid.size() == 36);
  DCHECK(uuid[8] == '-');
  DCHECK(uuid[13] == '-');
  DCHECK(uuid[18] == '-');
  DCHECK(uuid[23] == '-');
  std::string numbers_only = uuid;
  numbers_only.erase(23, 1);
  numbers_only.erase(18, 1);
  numbers_only.erase(13, 1);
  numbers_only.erase(8, 1);
  std::vector<uint8> uuid_bytes_vector;
  base::HexStringToBytes(numbers_only, &uuid_bytes_vector);
  DCHECK(uuid_bytes_vector.size() == 16);

  return [IOBluetoothSDPUUID uuidWithBytes:&uuid_bytes_vector[0]
                                    length:uuid_bytes_vector.size()];
}

}  // namespace

namespace device {

BluetoothProfileMac::BluetoothProfileMac(const BluetoothUUID& uuid,
                                         const std::string& name)
    : BluetoothProfile(), uuid_(uuid), name_(name) {
}

BluetoothProfileMac::~BluetoothProfileMac() {
}

void BluetoothProfileMac::Unregister() {
  delete this;
}

void BluetoothProfileMac::SetConnectionCallback(
    const ConnectionCallback& callback) {
  connection_callback_ = callback;
}

bool BluetoothProfileMac::Connect(IOBluetoothDevice* device) {
  if (connection_callback_.is_null())
    return false;

  IOBluetoothSDPServiceRecord* record =
      [device getServiceRecordForUUID:GetIOBluetoothSDPUUID(
          uuid_.canonical_value())];
  if (record != nil) {
    scoped_refptr<BluetoothSocket> socket(
        BluetoothSocketMac::CreateBluetoothSocket(record));
    if (socket.get() != NULL) {
      BluetoothDeviceMac device_mac(device);
      connection_callback_.Run(&device_mac, socket);
      return true;
    }
  }
  return false;
}

}  // namespace device
