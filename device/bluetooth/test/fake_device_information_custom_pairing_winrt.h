// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_FAKE_DEVICE_INFORMATION_CUSTOM_PAIRING_WINRT_H_
#define DEVICE_BLUETOOTH_TEST_FAKE_DEVICE_INFORMATION_CUSTOM_PAIRING_WINRT_H_

#include <windows.devices.enumeration.h>
#include <wrl/implements.h>

#include "base/macros.h"

namespace device {

class FakeDeviceInformationCustomPairingWinrt
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::Devices::Enumeration::IDeviceInformationCustomPairing> {
 public:
  FakeDeviceInformationCustomPairingWinrt();
  ~FakeDeviceInformationCustomPairingWinrt() override;

  // IDeviceInformationCustomPairing:
  IFACEMETHODIMP PairAsync(
      ABI::Windows::Devices::Enumeration::DevicePairingKinds
          pairing_kinds_supported,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Enumeration::DevicePairingResult*>** result)
      override;
  IFACEMETHODIMP PairWithProtectionLevelAsync(
      ABI::Windows::Devices::Enumeration::DevicePairingKinds
          pairing_kinds_supported,
      ABI::Windows::Devices::Enumeration::DevicePairingProtectionLevel
          min_protection_level,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Enumeration::DevicePairingResult*>** result)
      override;
  IFACEMETHODIMP PairWithProtectionLevelAndSettingsAsync(
      ABI::Windows::Devices::Enumeration::DevicePairingKinds
          pairing_kinds_supported,
      ABI::Windows::Devices::Enumeration::DevicePairingProtectionLevel
          min_protection_level,
      ABI::Windows::Devices::Enumeration::IDevicePairingSettings*
          device_pairing_settings,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Enumeration::DevicePairingResult*>** result)
      override;
  IFACEMETHODIMP add_PairingRequested(
      ABI::Windows::Foundation::ITypedEventHandler<
          ABI::Windows::Devices::Enumeration::DeviceInformationCustomPairing*,
          ABI::Windows::Devices::Enumeration::DevicePairingRequestedEventArgs*>*
          handler,
      EventRegistrationToken* token) override;
  IFACEMETHODIMP remove_PairingRequested(EventRegistrationToken token) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeDeviceInformationCustomPairingWinrt);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_FAKE_DEVICE_INFORMATION_CUSTOM_PAIRING_WINRT_H_
