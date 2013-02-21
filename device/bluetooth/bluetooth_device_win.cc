// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TODO(youngki): Implement this file.

#include "device/bluetooth/bluetooth_device_win.h"

#include <string>

#include "base/basictypes.h"
#include "base/hash.h"
#include "base/logging.h"
#include "base/memory/scoped_vector.h"
#include "base/stringprintf.h"
#include "device/bluetooth/bluetooth_out_of_band_pairing_data.h"
#include "device/bluetooth/bluetooth_service_record_win.h"

namespace {

const int kSdpBytesBufferSize = 1024;

}  // namespace

namespace device {

BluetoothDeviceWin::BluetoothDeviceWin(
    const BluetoothTaskManagerWin::DeviceState& state)
    : BluetoothDevice(), device_fingerprint_(ComputeDeviceFingerprint(state)) {
  name_ = state.name;
  address_ = state.address;
  bluetooth_class_ = state.bluetooth_class;
  connected_ = state.connected;
  bonded_ = state.authenticated;

  for (ScopedVector<BluetoothTaskManagerWin::ServiceRecordState>::const_iterator
       iter = state.service_record_states.begin();
       iter != state.service_record_states.end();
       ++iter) {
    uint8 sdp_bytes_buffer[kSdpBytesBufferSize];
    std::copy((*iter)->sdp_bytes.begin(),
              (*iter)->sdp_bytes.end(),
              sdp_bytes_buffer);
    BluetoothServiceRecord* service_record = new BluetoothServiceRecordWin(
        (*iter)->name,
        (*iter)->address,
        (*iter)->sdp_bytes.size(),
        sdp_bytes_buffer);
    service_record_list_.push_back(service_record);
    service_uuids_.push_back(service_record->uuid());
  }
}

BluetoothDeviceWin::~BluetoothDeviceWin() {
}

void BluetoothDeviceWin::SetVisible(bool visible) {
  visible_ = visible;
}

bool BluetoothDeviceWin::IsPaired() const {
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

// static
uint32 BluetoothDeviceWin::ComputeDeviceFingerprint(
    const BluetoothTaskManagerWin::DeviceState& state) {
  std::string device_string = base::StringPrintf("%s%s%u%s%s%s",
      state.name.c_str(),
      state.address.c_str(),
      state.bluetooth_class,
      state.visible ? "true" : "false",
      state.connected ? "true" : "false",
      state.authenticated ? "true" : "false");
  for (ScopedVector<BluetoothTaskManagerWin::ServiceRecordState>::const_iterator
      iter = state.service_record_states.begin();
      iter != state.service_record_states.end();
      ++iter) {
    base::StringAppendF(&device_string,
                        "%s%s%d",
                        (*iter)->name.c_str(),
                        (*iter)->address.c_str(),
                        (*iter)->sdp_bytes.size());
  }
  return base::Hash(device_string);
}

}  // namespace device
