// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_device_mac.h"

#include <string>

#include "base/basictypes.h"
#include "base/hash.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "device/bluetooth/bluetooth_out_of_band_pairing_data.h"
#include "device/bluetooth/bluetooth_profile_mac.h"
#include "device/bluetooth/bluetooth_service_record_mac.h"
#include "device/bluetooth/bluetooth_socket_mac.h"
#include "device/bluetooth/bluetooth_uuid.h"

// Replicate specific 10.7 SDK declarations for building with prior SDKs.
#if !defined(MAC_OS_X_VERSION_10_7) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_7

@interface IOBluetoothDevice (LionSDKDeclarations)
- (NSString*)addressString;
- (unsigned int)classOfDevice;
- (BluetoothConnectionHandle)connectionHandle;
- (BluetoothHCIRSSIValue)rawRSSI;
- (NSArray*)services;
@end

#endif  // MAC_OS_X_VERSION_10_7

// Undocumented API for accessing the Bluetooth transmit power level.
// Similar to the API defined here [ http://goo.gl/20Q5vE ].
@interface IOBluetoothHostController (UndocumentedAPI)
- (IOReturn)
    BluetoothHCIReadTransmitPowerLevel:(BluetoothConnectionHandle)connection
                                inType:(BluetoothHCITransmitPowerLevelType)type
                 outTransmitPowerLevel:(BluetoothHCITransmitPowerLevel*)level;
@end

namespace device {

BluetoothDeviceMac::BluetoothDeviceMac(IOBluetoothDevice* device)
    : device_([device retain]) {
}

BluetoothDeviceMac::~BluetoothDeviceMac() {
}

void BluetoothDeviceMac::AddObserver(
    device::BluetoothDevice::Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void BluetoothDeviceMac::RemoveObserver(
    device::BluetoothDevice::Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

uint32 BluetoothDeviceMac::GetBluetoothClass() const {
  return [device_ classOfDevice];
}

std::string BluetoothDeviceMac::GetDeviceName() const {
  return base::SysNSStringToUTF8([device_ name]);
}

std::string BluetoothDeviceMac::GetAddress() const {
  return GetDeviceAddress(device_);
}

BluetoothDevice::VendorIDSource BluetoothDeviceMac::GetVendorIDSource() const {
  return VENDOR_ID_UNKNOWN;
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

int BluetoothDeviceMac::GetRSSI() const {
  if (![device_ isConnected]) {
    NOTIMPLEMENTED();
    return kUnknownPower;
  }

  int rssi = [device_ rawRSSI];

  // The API guarantees that +127 is returned in case the RSSI is not readable:
  // http://goo.gl/bpURYv
  if (rssi == 127)
    return kUnknownPower;

  return rssi;
}

int BluetoothDeviceMac::GetCurrentHostTransmitPower() const {
  return GetHostTransmitPower(kReadCurrentTransmitPowerLevel);
}

int BluetoothDeviceMac::GetMaximumHostTransmitPower() const {
  return GetHostTransmitPower(kReadMaximumTransmitPowerLevel);
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

// TODO(keybuk): BluetoothServiceRecord is deprecated; implement this method
// without using BluetoothServiceRecord.
BluetoothDevice::UUIDList BluetoothDeviceMac::GetUUIDs() const {
  UUIDList uuids;
  for (IOBluetoothSDPServiceRecord* service in [device_ services]) {
    BluetoothServiceRecordMac service_record(service);
    uuids.push_back(service_record.uuid());
  }
  return uuids;
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

void BluetoothDeviceMac::Disconnect(const base::Closure& callback,
                                    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceMac::Forget(const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceMac::ConnectToProfile(
    BluetoothProfile* profile,
    const base::Closure& callback,
    const ConnectToProfileErrorCallback& error_callback) {
  static_cast<BluetoothProfileMac*>(profile)
      ->Connect(device_, callback, error_callback);
}

void BluetoothDeviceMac::ConnectToService(
    const BluetoothUUID& uuid,
    const ConnectToServiceCallback& callback,
    const ConnectToServiceErrorCallback& error_callback) {
  // TODO(keybuk): implement
  NOTIMPLEMENTED();
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

void BluetoothDeviceMac::StartConnectionMonitor(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

int BluetoothDeviceMac::GetHostTransmitPower(
    BluetoothHCITransmitPowerLevelType power_level_type) const {
  IOBluetoothHostController* controller =
      [IOBluetoothHostController defaultController];

  // Bail if the undocumented API is unavailable on this machine.
  SEL selector = @selector(
      BluetoothHCIReadTransmitPowerLevel:inType:outTransmitPowerLevel:);
  if (![controller respondsToSelector:selector])
    return kUnknownPower;

  BluetoothHCITransmitPowerLevel power_level;
  IOReturn result =
      [controller BluetoothHCIReadTransmitPowerLevel:[device_ connectionHandle]
                                              inType:power_level_type
                               outTransmitPowerLevel:&power_level];
  if (result != kIOReturnSuccess)
    return kUnknownPower;

  return power_level;
}

// static
std::string BluetoothDeviceMac::GetDeviceAddress(IOBluetoothDevice* device) {
  return CanonicalizeAddress(base::SysNSStringToUTF8([device addressString]));
}

}  // namespace device
