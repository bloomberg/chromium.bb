// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_device_mac.h"

#include <string>

#include "base/basictypes.h"
#include "device/bluetooth/bluetooth_out_of_band_pairing_data.h"
#include "device/bluetooth/bluetooth_service_record_mac.h"

namespace device {

BluetoothDeviceMac::BluetoothDeviceMac()
    : BluetoothDevice() {
}

BluetoothDeviceMac::~BluetoothDeviceMac() {
}

bool BluetoothDeviceMac::IsPaired() const {
  return false;
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

}  // namespace device
