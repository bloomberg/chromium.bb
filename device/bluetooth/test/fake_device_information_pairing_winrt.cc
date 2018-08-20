// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/fake_device_information_pairing_winrt.h"

#include <utility>

#include "device/bluetooth/test/fake_device_information_custom_pairing_winrt.h"

namespace device {

namespace {

using ABI::Windows::Devices::Enumeration::DevicePairingProtectionLevel;
using ABI::Windows::Devices::Enumeration::DevicePairingResult;
using ABI::Windows::Devices::Enumeration::DeviceUnpairingResult;
using ABI::Windows::Devices::Enumeration::IDeviceInformationCustomPairing;
using ABI::Windows::Devices::Enumeration::IDevicePairingSettings;
using ABI::Windows::Foundation::IAsyncOperation;
using Microsoft::WRL::Make;

}  // namespace

FakeDeviceInformationPairingWinrt::FakeDeviceInformationPairingWinrt(
    bool is_paired)
    : is_paired_(is_paired) {}

FakeDeviceInformationPairingWinrt::FakeDeviceInformationPairingWinrt(
    std::string pin)
    : custom_(Make<FakeDeviceInformationCustomPairingWinrt>(this,
                                                            std::move(pin))) {}

FakeDeviceInformationPairingWinrt::~FakeDeviceInformationPairingWinrt() =
    default;

HRESULT FakeDeviceInformationPairingWinrt::get_IsPaired(boolean* value) {
  *value = is_paired_;
  return S_OK;
}

HRESULT FakeDeviceInformationPairingWinrt::get_CanPair(boolean* value) {
  return E_NOTIMPL;
}

HRESULT FakeDeviceInformationPairingWinrt::PairAsync(
    IAsyncOperation<DevicePairingResult*>** result) {
  return E_NOTIMPL;
}

HRESULT FakeDeviceInformationPairingWinrt::PairWithProtectionLevelAsync(
    DevicePairingProtectionLevel min_protection_level,
    IAsyncOperation<DevicePairingResult*>** result) {
  return E_NOTIMPL;
}

HRESULT FakeDeviceInformationPairingWinrt::get_ProtectionLevel(
    DevicePairingProtectionLevel* value) {
  return E_NOTIMPL;
}

HRESULT FakeDeviceInformationPairingWinrt::get_Custom(
    IDeviceInformationCustomPairing** value) {
  return custom_.CopyTo(value);
}

HRESULT
FakeDeviceInformationPairingWinrt::PairWithProtectionLevelAndSettingsAsync(
    DevicePairingProtectionLevel min_protection_level,
    IDevicePairingSettings* device_pairing_settings,
    IAsyncOperation<DevicePairingResult*>** result) {
  return E_NOTIMPL;
}

HRESULT FakeDeviceInformationPairingWinrt::UnpairAsync(
    IAsyncOperation<DeviceUnpairingResult*>** result) {
  return E_NOTIMPL;
}

}  // namespace device
