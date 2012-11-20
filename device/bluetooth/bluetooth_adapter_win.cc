// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TODO(youngki): Implement this file.

#include "device/bluetooth/bluetooth_adapter_win.h"

#include <BluetoothAPIs.h>
#include <string>
#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/stringprintf.h"
#include "base/sys_string_conversions.h"

# pragma comment(lib, "Bthprops.lib")

namespace {

const BLUETOOTH_FIND_RADIO_PARAMS bluetooth_adapter_param =
    { sizeof(BLUETOOTH_FIND_RADIO_PARAMS) };

}  // namespace

namespace device {

const int BluetoothAdapterWin::kPollIntervalMs = 500;

BluetoothAdapterWin::BluetoothAdapterWin()
    : BluetoothAdapter(),
      powered_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
}

BluetoothAdapterWin::~BluetoothAdapterWin() {
}

void BluetoothAdapterWin::AddObserver(BluetoothAdapter::Observer* observer) {
  NOTIMPLEMENTED();
}

void BluetoothAdapterWin::RemoveObserver(BluetoothAdapter::Observer* observer) {
  NOTIMPLEMENTED();
}

bool BluetoothAdapterWin::IsPresent() const {
  return !address_.empty();
}

bool BluetoothAdapterWin::IsPowered() const {
  return powered_;
}

void BluetoothAdapterWin::SetPowered(
    bool powered,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

bool BluetoothAdapterWin::IsDiscovering() const {
  NOTIMPLEMENTED();
  return false;
}

void BluetoothAdapterWin::SetDiscovering(
    bool discovering,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

BluetoothAdapter::ConstDeviceList BluetoothAdapterWin::GetDevices() const {
  NOTIMPLEMENTED();
  return BluetoothAdapter::ConstDeviceList();
}

BluetoothDevice* BluetoothAdapterWin::GetDevice(const std::string& address) {
  NOTIMPLEMENTED();
  return NULL;
}

const BluetoothDevice* BluetoothAdapterWin::GetDevice(
    const std::string& address) const {
  NOTIMPLEMENTED();
  return NULL;
}

void BluetoothAdapterWin::ReadLocalOutOfBandPairingData(
    const BluetoothOutOfBandPairingDataCallback& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothAdapterWin::UpdateAdapterState() {
  HBLUETOOTH_RADIO_FIND bluetooth_adapter_handle = NULL;
  BLUETOOTH_RADIO_INFO bluetooth_adapter_info =
      { sizeof(BLUETOOTH_RADIO_INFO), 0 };
  HBLUETOOTH_RADIO_FIND bluetooth_handle = BluetoothFindFirstRadio(
      &bluetooth_adapter_param, &bluetooth_adapter_handle);

  if (bluetooth_adapter_handle) {
    if (ERROR_SUCCESS == BluetoothGetRadioInfo(bluetooth_adapter_handle,
                                               &bluetooth_adapter_info)) {
      name_ = base::SysWideToUTF8(bluetooth_adapter_info.szName);
      address_ = base::StringPrintf("%02X:%02X:%02X:%02X:%02X:%02X",
          bluetooth_adapter_info.address.rgBytes[5],
          bluetooth_adapter_info.address.rgBytes[4],
          bluetooth_adapter_info.address.rgBytes[3],
          bluetooth_adapter_info.address.rgBytes[2],
          bluetooth_adapter_info.address.rgBytes[1],
          bluetooth_adapter_info.address.rgBytes[0]);
      powered_ = BluetoothIsConnectable(bluetooth_adapter_handle) ||
          BluetoothIsDiscoverable(bluetooth_adapter_handle);
    } else {
      name_.clear();
      address_.clear();
      powered_ = false;
    }
  }

  if (bluetooth_handle)
    BluetoothFindRadioClose(bluetooth_handle);
}

void BluetoothAdapterWin::TrackDefaultAdapter() {
  PollAdapterState();
}

void BluetoothAdapterWin::PollAdapterState() {
  UpdateAdapterState();

  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&BluetoothAdapterWin::PollAdapterState,
                 weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(kPollIntervalMs));
}

}  // namespace device
