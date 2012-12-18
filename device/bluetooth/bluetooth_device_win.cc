// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TODO(youngki): Implement this file.

#include "device/bluetooth/bluetooth_device_win.h"

#include <string>
#include "base/basictypes.h"
#include "base/logging.h"
#include "device/bluetooth/bluetooth_out_of_band_pairing_data.h"

namespace device {

BluetoothDeviceWin::BluetoothDeviceWin() : BluetoothDevice() {
}

BluetoothDeviceWin::~BluetoothDeviceWin() {
}

bool BluetoothDeviceWin::IsPaired() const {
  NOTIMPLEMENTED();
  return false;
}

const BluetoothDevice::ServiceList& BluetoothDeviceWin::GetServices() const {
  NOTIMPLEMENTED();
  return service_uuids_;
}

void BluetoothDeviceWin::GetServiceRecords(
    const ServiceRecordsCallback& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

bool BluetoothDeviceWin::ProvidesServiceWithUUID(
    const std::string& uuid) const {
  NOTIMPLEMENTED();
  return false;
}

void BluetoothDeviceWin::ProvidesServiceWithName(
    const std::string& name,
    const ProvidesServiceCallback& callback) {
  NOTIMPLEMENTED();
}

bool BluetoothDeviceWin::ExpectingPinCode() const {
  NOTIMPLEMENTED();
  return false;
}

bool BluetoothDeviceWin::ExpectingPasskey() const {
  NOTIMPLEMENTED();
  return false;
}

bool BluetoothDeviceWin::ExpectingConfirmation() const {
  NOTIMPLEMENTED();
  return false;
}

void BluetoothDeviceWin::Connect(
    PairingDelegate* pairing_delegate,
    const base::Closure& callback,
    const ConnectErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceWin::SetPinCode(const std::string& pincode) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceWin::SetPasskey(uint32 passkey) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceWin::ConfirmPairing() {
  NOTIMPLEMENTED();
}

void BluetoothDeviceWin::RejectPairing() {
  NOTIMPLEMENTED();
}

void BluetoothDeviceWin::CancelPairing() {
  NOTIMPLEMENTED();
}

void BluetoothDeviceWin::Disconnect(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceWin::Forget(const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceWin::ConnectToService(
    const std::string& service_uuid,
    const SocketCallback& callback) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceWin::SetOutOfBandPairingData(
    const BluetoothOutOfBandPairingData& data,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceWin::ClearOutOfBandPairingData(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

}  // namespace device
