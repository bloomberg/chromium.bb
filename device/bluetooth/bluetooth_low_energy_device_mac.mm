// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_low_energy_device_mac.h"

#import <CoreFoundation/CoreFoundation.h>

#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/sdk_forward_declarations.h"
#include "base/strings/sys_string_conversions.h"
#include "device/bluetooth/bluetooth_adapter_mac.h"

using device::BluetoothDevice;
using device::BluetoothLowEnergyDeviceMac;

namespace {

// Converts a CBUUID to a BluetoothUUID.
device::BluetoothUUID BluetoothUUIDWithCBUUID(CBUUID* uuid) {
  // UUIDString only available OS X >= 10.8.
  DCHECK(base::mac::IsOSMountainLionOrLater());
  std::string uuid_c_string = base::SysNSStringToUTF8([uuid UUIDString]);
  return device::BluetoothUUID(uuid_c_string);
}

}  // namespace

BluetoothLowEnergyDeviceMac::BluetoothLowEnergyDeviceMac(
    CBPeripheral* peripheral,
    NSDictionary* advertisementData,
    int rssi) {
  DCHECK(BluetoothAdapterMac::IsLowEnergyAvailable());
  Update(peripheral, advertisementData, rssi);
}

BluetoothLowEnergyDeviceMac::~BluetoothLowEnergyDeviceMac() {
}

void BluetoothLowEnergyDeviceMac::Update(CBPeripheral* peripheral,
                                         NSDictionary* advertisementData,
                                         int rssi) {
  last_update_time_.reset([[NSDate date] retain]);
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

int16 BluetoothLowEnergyDeviceMac::GetInquiryRSSI() const {
  return kUnknownPower;
}

int16 BluetoothLowEnergyDeviceMac::GetInquiryTxPower() const {
  NOTIMPLEMENTED();
  return kUnknownPower;
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

NSDate* BluetoothLowEnergyDeviceMac::GetLastUpdateTime() const {
  return last_update_time_.get();
}

std::string BluetoothLowEnergyDeviceMac::GetDeviceName() const {
  return base::SysNSStringToUTF8([peripheral_ name]);
}

// static
std::string BluetoothLowEnergyDeviceMac::GetPeripheralIdentifier(
    CBPeripheral* peripheral) {
  DCHECK(BluetoothAdapterMac::IsLowEnergyAvailable());
  NSUUID* uuid = [peripheral identifier];
  NSString* uuidString = [uuid UUIDString];
  return base::SysNSStringToUTF8(uuidString);
}
