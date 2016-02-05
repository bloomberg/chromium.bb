// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_classic_win_fake.h"

#include "base/logging.h"

namespace device {
namespace win {

BluetoothClassicWrapperFake::BluetoothClassicWrapperFake() {}
BluetoothClassicWrapperFake::~BluetoothClassicWrapperFake() {}

HBLUETOOTH_RADIO_FIND BluetoothClassicWrapperFake::FindFirstRadio(
    const BLUETOOTH_FIND_RADIO_PARAMS* params,
    HANDLE* out_handle) {
  NOTIMPLEMENTED();
  return NULL;
}

DWORD BluetoothClassicWrapperFake::GetRadioInfo(
    HANDLE handle,
    PBLUETOOTH_RADIO_INFO out_radio_info) {
  NOTIMPLEMENTED();
  return ERROR_SUCCESS;
}

BOOL BluetoothClassicWrapperFake::FindRadioClose(HBLUETOOTH_RADIO_FIND handle) {
  NOTIMPLEMENTED();
  return TRUE;
}

BOOL BluetoothClassicWrapperFake::IsConnectable(HANDLE handle) {
  NOTIMPLEMENTED();
  return TRUE;
}

HBLUETOOTH_DEVICE_FIND BluetoothClassicWrapperFake::FindFirstDevice(
    const BLUETOOTH_DEVICE_SEARCH_PARAMS* params,
    BLUETOOTH_DEVICE_INFO* out_device_info) {
  NOTIMPLEMENTED();
  return NULL;
}

BOOL BluetoothClassicWrapperFake::FindNextDevice(
    HBLUETOOTH_DEVICE_FIND handle,
    BLUETOOTH_DEVICE_INFO* out_device_info) {
  NOTIMPLEMENTED();
  return TRUE;
}

BOOL BluetoothClassicWrapperFake::FindDeviceClose(
    HBLUETOOTH_DEVICE_FIND handle) {
  NOTIMPLEMENTED();
  return TRUE;
}

BOOL BluetoothClassicWrapperFake::EnableDiscovery(HANDLE handle,
                                                  BOOL is_enable) {
  NOTIMPLEMENTED();
  return TRUE;
}

BOOL BluetoothClassicWrapperFake::EnableIncomingConnections(HANDLE handle,
                                                            BOOL is_enable) {
  NOTIMPLEMENTED();
  return TRUE;
}
}  // namespace device
}  // namespace win
