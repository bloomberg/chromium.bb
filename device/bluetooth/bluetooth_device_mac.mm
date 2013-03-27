// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_device_mac.h"

#include <IOBluetooth/Bluetooth.h>
#import <IOBluetooth/objc/IOBluetoothDevice.h>
#import <IOBluetooth/objc/IOBluetoothSDPServiceRecord.h>

#include <string>

#include "base/basictypes.h"
#include "base/hash.h"
#include "base/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "device/bluetooth/bluetooth_out_of_band_pairing_data.h"
#include "device/bluetooth/bluetooth_service_record_mac.h"

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

namespace device {

BluetoothDeviceMac::BluetoothDeviceMac(const IOBluetoothDevice* device)
    : BluetoothDevice(),
      device_fingerprint_(ComputeDeviceFingerprint(device)) {
  name_ = base::SysNSStringToUTF8([device name]);
  address_ = base::SysNSStringToUTF8([device addressString]);
  bluetooth_class_ = [device classOfDevice];
  connected_ = [device isConnected];
  bonded_ = [device isPaired];
  visible_ = true;
}

BluetoothDeviceMac::~BluetoothDeviceMac() {
}

bool BluetoothDeviceMac::IsPaired() const {
  return bonded_;
}

const BluetoothDevice::ServiceList& BluetoothDeviceMac::GetServices() const {
  return service_uuids_;
}

void BluetoothDeviceMac::GetServiceRecords(
    const ServiceRecordsCallback& callback,
    const ErrorCallback& error_callback) {
}

void BluetoothDeviceMac::ProvidesServiceWithName(
    const std::string& name,
    const ProvidesServiceCallback& callback) {
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

// static
uint32 BluetoothDeviceMac::ComputeDeviceFingerprint(
    const IOBluetoothDevice* device) {
  std::string device_string = base::StringPrintf("%s|%s|%u|%d|%d",
      base::SysNSStringToUTF8([device name]).c_str(),
      base::SysNSStringToUTF8([device addressString]).c_str(),
      [device classOfDevice],
      [device isConnected],
      [device isPaired]);

  for (IOBluetoothSDPServiceRecord* record in [device services]) {
    base::StringAppendF(
        &device_string,
        "|%s|%lu",
        base::SysNSStringToUTF8([record getServiceName]).c_str(),
        static_cast<unsigned long>([[record attributes] count]));
  }

  return base::Hash(device_string);
}

}  // namespace device
