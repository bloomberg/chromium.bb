// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/fake_gatt_descriptors_result_winrt.h"

namespace device {

namespace {

using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCommunicationStatus;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::GattDescriptor;
using ABI::Windows::Foundation::Collections::IVectorView;
using ABI::Windows::Foundation::IReference;

}  // namespace

FakeGattDescriptorsResultWinrt::FakeGattDescriptorsResultWinrt() = default;

FakeGattDescriptorsResultWinrt::~FakeGattDescriptorsResultWinrt() = default;

HRESULT FakeGattDescriptorsResultWinrt::get_Status(
    GattCommunicationStatus* value) {
  return E_NOTIMPL;
}

HRESULT FakeGattDescriptorsResultWinrt::get_ProtocolError(
    IReference<uint8_t>** value) {
  return E_NOTIMPL;
}

HRESULT FakeGattDescriptorsResultWinrt::get_Descriptors(
    IVectorView<GattDescriptor*>** value) {
  return E_NOTIMPL;
}

}  // namespace device
