// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_classic_win.h"

namespace {
static device::win::BluetoothClassicWrapper* g_instance_ = nullptr;
}  // namespace

namespace device {
namespace win {

BluetoothClassicWrapper* BluetoothClassicWrapper::GetInstance() {
  if (g_instance_ == nullptr) {
    g_instance_ = new BluetoothClassicWrapper();
  }
  return g_instance_;
}

void BluetoothClassicWrapper::DeleteInstance() {
  delete g_instance_;
  g_instance_ = nullptr;
}

void BluetoothClassicWrapper::SetInstanceForTest(
    BluetoothClassicWrapper* instance) {
  delete g_instance_;
  g_instance_ = instance;
}

BluetoothClassicWrapper::BluetoothClassicWrapper() {}
BluetoothClassicWrapper::~BluetoothClassicWrapper() {}

HBLUETOOTH_RADIO_FIND BluetoothClassicWrapper::FindFirstRadio(
    const BLUETOOTH_FIND_RADIO_PARAMS* params,
    HANDLE* out_handle) {
  return BluetoothFindFirstRadio(params, out_handle);
}

DWORD BluetoothClassicWrapper::GetRadioInfo(
    HANDLE handle,
    PBLUETOOTH_RADIO_INFO out_radio_info) {
  return BluetoothGetRadioInfo(handle, out_radio_info);
}

BOOL BluetoothClassicWrapper::FindRadioClose(HBLUETOOTH_RADIO_FIND handle) {
  return BluetoothFindRadioClose(handle);
}

BOOL BluetoothClassicWrapper::IsConnectable(HANDLE handle) {
  return BluetoothIsConnectable(handle);
}

HBLUETOOTH_DEVICE_FIND BluetoothClassicWrapper::FindFirstDevice(
    const BLUETOOTH_DEVICE_SEARCH_PARAMS* params,
    BLUETOOTH_DEVICE_INFO* out_device_info) {
  return BluetoothFindFirstDevice(params, out_device_info);
}

BOOL BluetoothClassicWrapper::FindNextDevice(
    HBLUETOOTH_DEVICE_FIND handle,
    BLUETOOTH_DEVICE_INFO* out_device_info) {
  return BluetoothFindNextDevice(handle, out_device_info);
}

BOOL BluetoothClassicWrapper::FindDeviceClose(HBLUETOOTH_DEVICE_FIND handle) {
  return BluetoothFindDeviceClose(handle);
}

BOOL BluetoothClassicWrapper::EnableDiscovery(HANDLE handle, BOOL is_enable) {
  return BluetoothEnableDiscovery(handle, is_enable);
}

BOOL BluetoothClassicWrapper::EnableIncomingConnections(HANDLE handle,
                                                        BOOL is_enable) {
  return BluetoothEnableIncomingConnections(handle, is_enable);
}

}  // namespace win
}  // namespace device
