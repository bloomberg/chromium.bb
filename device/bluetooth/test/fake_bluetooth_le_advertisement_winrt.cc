// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/fake_bluetooth_le_advertisement_winrt.h"

namespace device {

namespace {

using ABI::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementDataSection;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementFlags;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEManufacturerData;
using ABI::Windows::Foundation::IReference;
using ABI::Windows::Foundation::Collections::IVector;
using ABI::Windows::Foundation::Collections::IVectorView;

}  // namespace

FakeBluetoothLEAdvertisementWinrt::FakeBluetoothLEAdvertisementWinrt() =
    default;

FakeBluetoothLEAdvertisementWinrt::~FakeBluetoothLEAdvertisementWinrt() =
    default;

HRESULT FakeBluetoothLEAdvertisementWinrt::get_Flags(
    IReference<BluetoothLEAdvertisementFlags>** value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEAdvertisementWinrt::put_Flags(
    IReference<BluetoothLEAdvertisementFlags>* value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEAdvertisementWinrt::get_LocalName(HSTRING* value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEAdvertisementWinrt::put_LocalName(HSTRING value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEAdvertisementWinrt::get_ServiceUuids(
    IVector<GUID>** value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEAdvertisementWinrt::get_ManufacturerData(
    IVector<BluetoothLEManufacturerData*>** value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEAdvertisementWinrt::get_DataSections(
    IVector<BluetoothLEAdvertisementDataSection*>** value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEAdvertisementWinrt::GetManufacturerDataByCompanyId(
    uint16_t company_id,
    IVectorView<BluetoothLEManufacturerData*>** data_list) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEAdvertisementWinrt::GetSectionsByType(
    uint8_t type,
    IVectorView<BluetoothLEAdvertisementDataSection*>** section_list) {
  return E_NOTIMPL;
}

}  // namespace device
