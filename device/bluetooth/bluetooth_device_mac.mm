// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_device_mac.h"

#include <IOBluetooth/Bluetooth.h>
#import <IOBluetooth/objc/IOBluetoothDevice.h>
#import <IOBluetooth/objc/IOBluetoothSDPServiceRecord.h>
#import <IOBluetooth/objc/IOBluetoothSDPUUID.h>

#include <string>

#include "base/basictypes.h"
#include "base/hash.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "device/bluetooth/bluetooth_out_of_band_pairing_data.h"
#include "device/bluetooth/bluetooth_profile_mac.h"
#include "device/bluetooth/bluetooth_service_record_mac.h"
#include "device/bluetooth/bluetooth_socket_mac.h"

// Replicate specific 10.7 SDK declarations for building with prior SDKs.
#if !defined(MAC_OS_X_VERSION_10_7) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_7

@interface IOBluetoothDevice (LionSDKDeclarations)
- (NSString*)addressString;
- (NSString*)name;
- (unsigned int)classOfDevice;
- (NSArray*)services;
@end

#endif  // MAC_OS_X_VERSION_10_7

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

BluetoothDeviceMac::BluetoothDeviceMac(IOBluetoothDevice* device)
    : BluetoothDevice(), device_([device retain]) {
}

BluetoothDeviceMac::~BluetoothDeviceMac() {
  [device_ release];
}

uint32 BluetoothDeviceMac::GetBluetoothClass() const {
  return [device_ classOfDevice];
}

std::string BluetoothDeviceMac::GetDeviceName() const {
  return base::SysNSStringToUTF8([device_ name]);
}

std::string BluetoothDeviceMac::GetAddress() const {
  return base::SysNSStringToUTF8([device_ addressString]);
}

uint16 BluetoothDeviceMac::GetVendorID() const {
  return 0;
}

uint16 BluetoothDeviceMac::GetProductID() const {
  return 0;
}

uint16 BluetoothDeviceMac::GetDeviceID() const {
  return 0;
}

bool BluetoothDeviceMac::IsPaired() const {
  return [device_ isPaired];
}

bool BluetoothDeviceMac::IsConnected() const {
  return [device_ isConnected];
}

bool BluetoothDeviceMac::IsConnectable() const {
  return false;
}

bool BluetoothDeviceMac::IsConnecting() const {
  return false;
}

// TODO(youngki): BluetoothServiceRecord is deprecated; implement this method
// without using BluetoothServiceRecord.
BluetoothDevice::ServiceList BluetoothDeviceMac::GetServices() const {
  ServiceList service_uuids;
  for (IOBluetoothSDPServiceRecord* service in [device_ services]) {
    BluetoothServiceRecordMac service_record(service);
    service_uuids.push_back(service_record.uuid());
  }
  return service_uuids;
}

// NOTE(youngki): This method is deprecated; it will be removed soon.
void BluetoothDeviceMac::GetServiceRecords(
    const ServiceRecordsCallback& callback,
    const ErrorCallback& error_callback) {
  ServiceRecordList service_record_list;
  for (IOBluetoothSDPServiceRecord* service in [device_ services]) {
    BluetoothServiceRecord* service_record =
        new BluetoothServiceRecordMac(service);
    service_record_list.push_back(service_record);
  }

  callback.Run(service_record_list);
}

// NOTE(youngki): This method is deprecated; it will be removed soon.
void BluetoothDeviceMac::ProvidesServiceWithName(
    const std::string& name,
    const ProvidesServiceCallback& callback) {
  NOTIMPLEMENTED();
  callback.Run(false);
}

bool BluetoothDeviceMac::ExpectingPinCode() const {
  NOTIMPLEMENTED();
  return false;
}

bool BluetoothDeviceMac::ExpectingPasskey() const {
  NOTIMPLEMENTED();
  return false;
}

bool BluetoothDeviceMac::ExpectingConfirmation() const {
  NOTIMPLEMENTED();
  return false;
}

void BluetoothDeviceMac::Connect(
    PairingDelegate* pairing_delegate,
    const base::Closure& callback,
    const ConnectErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceMac::SetPinCode(const std::string& pincode) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceMac::SetPasskey(uint32 passkey) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceMac::ConfirmPairing() {
  NOTIMPLEMENTED();
}

void BluetoothDeviceMac::RejectPairing() {
  NOTIMPLEMENTED();
}

void BluetoothDeviceMac::CancelPairing() {
  NOTIMPLEMENTED();
}

void BluetoothDeviceMac::Disconnect(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceMac::Forget(const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceMac::ConnectToService(
    const std::string& service_uuid,
    const SocketCallback& callback) {
  IOBluetoothSDPServiceRecord* record =
      [device_ getServiceRecordForUUID:GetIOBluetoothSDPUUID(service_uuid)];
  if (record != nil) {
    BluetoothServiceRecordMac service_record(record);
    scoped_refptr<BluetoothSocket> socket(
        BluetoothSocketMac::CreateBluetoothSocket(service_record));
    if (socket.get() != NULL)
      callback.Run(socket);
  }
}

void BluetoothDeviceMac::ConnectToProfile(
    device::BluetoothProfile* profile,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  if (static_cast<BluetoothProfileMac*>(profile)->Connect(device_))
    callback.Run();
  else
    error_callback.Run();
}

void BluetoothDeviceMac::SetOutOfBandPairingData(
    const BluetoothOutOfBandPairingData& data,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceMac::ClearOutOfBandPairingData(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

}  // namespace device
