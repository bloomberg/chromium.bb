// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_profile_mac.h"

#import <IOBluetooth/objc/IOBluetoothDevice.h>
#import <IOBluetooth/objc/IOBluetoothSDPServiceRecord.h>
#import <IOBluetooth/objc/IOBluetoothSDPUUID.h>

#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device_mac.h"
#include "device/bluetooth/bluetooth_socket_mac.h"

// Replicate specific 10.7 SDK declarations for building with prior SDKs.
#if !defined(MAC_OS_X_VERSION_10_7) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_7

@interface IOBluetoothDevice (LionSDKDeclarations)
- (NSString*)addressString;
@end

#endif  // MAC_OS_X_VERSION_10_7

namespace {

const char kNoConnectionCallback[] = "Connection callback not set";
const char kProfileNotFound[] = "Profile not found";

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

void OnConnectSuccessWithAdapter(
    const base::Closure& callback,
    const device::BluetoothProfileMac::ConnectionCallback& connection_callback,
    const std::string& device_address,
    scoped_refptr<device::BluetoothSocketMac> socket,
    scoped_refptr<device::BluetoothAdapter> adapter) {
  const device::BluetoothDevice* device = adapter->GetDevice(device_address);
  if (device) {
    connection_callback.Run(device, socket);
    callback.Run();
  }
}

void OnConnectSuccess(
    const base::Closure& callback,
    const device::BluetoothProfileMac::ConnectionCallback& connection_callback,
    const std::string& device_address,
    scoped_refptr<device::BluetoothSocketMac> socket) {
  device::BluetoothAdapterFactory::GetAdapter(
      base::Bind(&OnConnectSuccessWithAdapter,
                 callback,
                 connection_callback,
                 device_address,
                 socket));
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

void BluetoothProfileMac::Connect(
    IOBluetoothDevice* device,
    const base::Closure& success_callback,
    const BluetoothSocket::ErrorCompletionCallback& error_callback) {
  if (connection_callback_.is_null()) {
    error_callback.Run(kNoConnectionCallback);
    return;
  }

  IOBluetoothSDPServiceRecord* record = [device
      getServiceRecordForUUID:GetIOBluetoothSDPUUID(uuid_.canonical_value())];
  if (record == nil) {
    error_callback.Run(kProfileNotFound);
    return;
  }

  std::string device_address = base::SysNSStringToUTF8([device addressString]);
  scoped_refptr<BluetoothSocketMac> socket(
      BluetoothSocketMac::CreateBluetoothSocket(record));
  socket->Connect(
      base::Bind(OnConnectSuccess,
                 success_callback,
                 connection_callback_,
                 device_address,
                 socket),
      error_callback);
}

}  // namespace device
