// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/fake_device_information_custom_pairing_winrt.h"

#include <windows.foundation.h>

namespace device {

namespace {

using ABI::Windows::Devices::Enumeration::DeviceInformationCustomPairing;
using ABI::Windows::Devices::Enumeration::DevicePairingKinds;
using ABI::Windows::Devices::Enumeration::DevicePairingProtectionLevel;
using ABI::Windows::Devices::Enumeration::DevicePairingRequestedEventArgs;
using ABI::Windows::Devices::Enumeration::DevicePairingResult;
using ABI::Windows::Devices::Enumeration::IDevicePairingSettings;
using ABI::Windows::Foundation::IAsyncOperation;
using ABI::Windows::Foundation::ITypedEventHandler;

}  // namespace

FakeDeviceInformationCustomPairingWinrt::
    FakeDeviceInformationCustomPairingWinrt() = default;

FakeDeviceInformationCustomPairingWinrt::
    ~FakeDeviceInformationCustomPairingWinrt() = default;

HRESULT FakeDeviceInformationCustomPairingWinrt::PairAsync(
    DevicePairingKinds pairing_kinds_supported,
    IAsyncOperation<DevicePairingResult*>** result) {
  return E_NOTIMPL;
}

HRESULT FakeDeviceInformationCustomPairingWinrt::PairWithProtectionLevelAsync(
    DevicePairingKinds pairing_kinds_supported,
    DevicePairingProtectionLevel min_protection_level,
    IAsyncOperation<DevicePairingResult*>** result) {
  return E_NOTIMPL;
}

HRESULT FakeDeviceInformationCustomPairingWinrt::
    PairWithProtectionLevelAndSettingsAsync(
        DevicePairingKinds pairing_kinds_supported,
        DevicePairingProtectionLevel min_protection_level,
        IDevicePairingSettings* device_pairing_settings,
        IAsyncOperation<DevicePairingResult*>** result) {
  return E_NOTIMPL;
}

HRESULT FakeDeviceInformationCustomPairingWinrt::add_PairingRequested(
    ITypedEventHandler<DeviceInformationCustomPairing*,
                       DevicePairingRequestedEventArgs*>* handler,
    EventRegistrationToken* token) {
  return E_NOTIMPL;
}

HRESULT FakeDeviceInformationCustomPairingWinrt::remove_PairingRequested(
    EventRegistrationToken token) {
  return E_NOTIMPL;
}

}  // namespace device
