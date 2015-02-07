// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_low_energy_device_mac.h"

#import <CoreFoundation/CoreFoundation.h>

#include "base/mac/scoped_cftyperef.h"
#include "base/mac/sdk_forward_declarations.h"
#include "base/strings/sys_string_conversions.h"

using device::BluetoothDevice;
using device::BluetoothLowEnergyDeviceMac;

namespace {

// Converts a CBUUID to a Cocoa string.
//
// The string representation can have the following formats:
// - 16 bit:  xxxx
// - 128 bit: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
// CBUUID supports only 16 bits and 128 bits formats.
//
// In OSX < 10.10, -[uuid UUIDString] method is not implemented. It's why we
// need to provide this function.
NSString* stringWithCBUUID(CBUUID* uuid) {
  NSData* data = [uuid data];

  NSUInteger bytesToConvert = [data length];
  const unsigned char* uuidBytes = (const unsigned char*)[data bytes];
  NSMutableString* outputString = [NSMutableString stringWithCapacity:16];

  for (NSUInteger currentByteIndex = 0; currentByteIndex < bytesToConvert;
       currentByteIndex++) {
    switch (currentByteIndex) {
      case 3:
      case 5:
      case 7:
      case 9:
        [outputString appendFormat:@"%02x-", uuidBytes[currentByteIndex]];
        break;
      default:
        [outputString appendFormat:@"%02x", uuidBytes[currentByteIndex]];
    }
  }
  return outputString;
}

// Converts a CBUUID to a BluetoothUUID.
device::BluetoothUUID BluetoothUUIDWithCBUUID(CBUUID* uuid) {
  NSString* uuidString = nil;
  // TODO(dvh): Remove this once we moved to OSX SDK >= 10.10.
  if ([uuid respondsToSelector:@selector(UUIDString)]) {
    uuidString = [uuid UUIDString];
  } else {
    uuidString = stringWithCBUUID(uuid);
  }
  std::string uuid_c_string = base::SysNSStringToUTF8(uuidString);
  return device::BluetoothUUID(uuid_c_string);
}

}  // namespace

BluetoothLowEnergyDeviceMac::BluetoothLowEnergyDeviceMac(
    CBPeripheral* peripheral,
    NSDictionary* advertisementData,
    int rssi) {
  Update(peripheral, advertisementData, rssi);
}

BluetoothLowEnergyDeviceMac::~BluetoothLowEnergyDeviceMac() {
}

void BluetoothLowEnergyDeviceMac::Update(CBPeripheral* peripheral,
                                         NSDictionary* advertisementData,
                                         int rssi) {
  peripheral_.reset([peripheral retain]);
  rssi_ = rssi;
  ClearServiceData();
  NSNumber* nbConnectable =
      [advertisementData objectForKey:CBAdvertisementDataIsConnectable];
  connectable_ = [nbConnectable boolValue];
  NSDictionary* serviceData =
      [advertisementData objectForKey:CBAdvertisementDataServiceDataKey];
  for (CBUUID* uuid in serviceData) {
    NSData* data = [serviceData objectForKey:uuid];
    BluetoothUUID serviceUUID = BluetoothUUIDWithCBUUID(uuid);
    SetServiceData(serviceUUID, (const char*)[data bytes], [data length]);
  }
}

std::string BluetoothLowEnergyDeviceMac::GetIdentifier() const {
  return GetPeripheralIdentifier(peripheral_);
}

uint32 BluetoothLowEnergyDeviceMac::GetBluetoothClass() const {
  return 0;
}

std::string BluetoothLowEnergyDeviceMac::GetAddress() const {
  return std::string();
}

BluetoothDevice::VendorIDSource BluetoothLowEnergyDeviceMac::GetVendorIDSource()
    const {
  return VENDOR_ID_UNKNOWN;
}

uint16 BluetoothLowEnergyDeviceMac::GetVendorID() const {
  return 0;
}

uint16 BluetoothLowEnergyDeviceMac::GetProductID() const {
  return 0;
}

uint16 BluetoothLowEnergyDeviceMac::GetDeviceID() const {
  return 0;
}

int BluetoothLowEnergyDeviceMac::GetRSSI() const {
  return rssi_;
}

bool BluetoothLowEnergyDeviceMac::IsPaired() const {
  return false;
}

bool BluetoothLowEnergyDeviceMac::IsConnected() const {
  return [peripheral_ isConnected];
}

bool BluetoothLowEnergyDeviceMac::IsConnectable() const {
  return connectable_;
}

bool BluetoothLowEnergyDeviceMac::IsConnecting() const {
  return false;
}

BluetoothDevice::UUIDList BluetoothLowEnergyDeviceMac::GetUUIDs() const {
  return std::vector<device::BluetoothUUID>();
}

bool BluetoothLowEnergyDeviceMac::ExpectingPinCode() const {
  return false;
}

bool BluetoothLowEnergyDeviceMac::ExpectingPasskey() const {
  return false;
}

bool BluetoothLowEnergyDeviceMac::ExpectingConfirmation() const {
  return false;
}

void BluetoothLowEnergyDeviceMac::GetConnectionInfo(
    const ConnectionInfoCallback& callback) {
  NOTIMPLEMENTED();
}

void BluetoothLowEnergyDeviceMac::Connect(
    PairingDelegate* pairing_delegate,
    const base::Closure& callback,
    const ConnectErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothLowEnergyDeviceMac::SetPinCode(const std::string& pincode) {
  NOTIMPLEMENTED();
}

void BluetoothLowEnergyDeviceMac::SetPasskey(uint32 passkey) {
  NOTIMPLEMENTED();
}

void BluetoothLowEnergyDeviceMac::ConfirmPairing() {
  NOTIMPLEMENTED();
}

void BluetoothLowEnergyDeviceMac::RejectPairing() {
  NOTIMPLEMENTED();
}

void BluetoothLowEnergyDeviceMac::CancelPairing() {
  NOTIMPLEMENTED();
}

void BluetoothLowEnergyDeviceMac::Disconnect(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothLowEnergyDeviceMac::Forget(const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothLowEnergyDeviceMac::ConnectToService(
    const BluetoothUUID& uuid,
    const ConnectToServiceCallback& callback,
    const ConnectToServiceErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothLowEnergyDeviceMac::ConnectToServiceInsecurely(
    const device::BluetoothUUID& uuid,
    const ConnectToServiceCallback& callback,
    const ConnectToServiceErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothLowEnergyDeviceMac::CreateGattConnection(
    const GattConnectionCallback& callback,
    const ConnectErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

std::string BluetoothLowEnergyDeviceMac::GetDeviceName() const {
  return base::SysNSStringToUTF8([peripheral_ name]);
}

std::string BluetoothLowEnergyDeviceMac::GetPeripheralIdentifier(
    CBPeripheral* peripheral) {
  // TODO(dvh): Remove this once we moved to OSX SDK >= 10.9.
  if ([peripheral respondsToSelector:@selector(identifier)]) {
    // When -[CBPeripheral identifier] is available.
    NSUUID* uuid = [peripheral identifier];
    NSString* uuidString = [uuid UUIDString];
    return base::SysNSStringToUTF8(uuidString);
  }

  base::ScopedCFTypeRef<CFStringRef> str(
      CFUUIDCreateString(NULL, [peripheral UUID]));
  return SysCFStringRefToUTF8(str);
}
