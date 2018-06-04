// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/fake_bluetooth_le_advertisement_received_event_args_winrt.h"

namespace device {

namespace {

using ABI::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementType;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    IBluetoothLEAdvertisement;
using ABI::Windows::Devices::Bluetooth::IBluetoothSignalStrengthFilter;
using ABI::Windows::Foundation::DateTime;
using Microsoft::WRL::ComPtr;

}  // namespace

FakeBluetoothLEAdvertisementReceivedEventArgsWinrt::
    FakeBluetoothLEAdvertisementReceivedEventArgsWinrt() = default;

FakeBluetoothLEAdvertisementReceivedEventArgsWinrt::
    ~FakeBluetoothLEAdvertisementReceivedEventArgsWinrt() = default;

HRESULT
FakeBluetoothLEAdvertisementReceivedEventArgsWinrt::get_RawSignalStrengthInDBm(
    int16_t* value) {
  return E_NOTIMPL;
}

HRESULT
FakeBluetoothLEAdvertisementReceivedEventArgsWinrt::get_BluetoothAddress(
    uint64_t* value) {
  return E_NOTIMPL;
}

HRESULT
FakeBluetoothLEAdvertisementReceivedEventArgsWinrt::get_AdvertisementType(
    BluetoothLEAdvertisementType* value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEAdvertisementReceivedEventArgsWinrt::get_Timestamp(
    DateTime* value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEAdvertisementReceivedEventArgsWinrt::get_Advertisement(
    IBluetoothLEAdvertisement** value) {
  return E_NOTIMPL;
}

}  // namespace device
