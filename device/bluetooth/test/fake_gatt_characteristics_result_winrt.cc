// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/fake_gatt_characteristics_result_winrt.h"

namespace device {

namespace {

using ABI::Windows::Foundation::Collections::IVectorView;
using ABI::Windows::Foundation::IReference;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCharacteristic;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCommunicationStatus;

}  // namespace

FakeGattCharacteristicsResultWinrt::FakeGattCharacteristicsResultWinrt() =
    default;

FakeGattCharacteristicsResultWinrt::~FakeGattCharacteristicsResultWinrt() =
    default;

HRESULT FakeGattCharacteristicsResultWinrt::get_Status(
    GattCommunicationStatus* value) {
  return E_NOTIMPL;
}

HRESULT FakeGattCharacteristicsResultWinrt::get_ProtocolError(
    IReference<uint8_t>** value) {
  return E_NOTIMPL;
}

HRESULT FakeGattCharacteristicsResultWinrt::get_Characteristics(
    IVectorView<GattCharacteristic*>** value) {
  return E_NOTIMPL;
}

}  // namespace device
