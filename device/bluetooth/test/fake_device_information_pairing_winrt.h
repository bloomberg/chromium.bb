// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_FAKE_DEVICE_INFORMATION_PAIRING_WINRT_H_
#define DEVICE_BLUETOOTH_TEST_FAKE_DEVICE_INFORMATION_PAIRING_WINRT_H_

#include <windows.devices.enumeration.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include "base/macros.h"

namespace device {

class FakeDeviceInformationPairingWinrt
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::Devices::Enumeration::IDeviceInformationPairing> {
 public:
  explicit FakeDeviceInformationPairingWinrt(bool is_paired);
  ~FakeDeviceInformationPairingWinrt() override;

  // IDeviceInformationPairing:
  IFACEMETHODIMP get_IsPaired(boolean* value) override;
  IFACEMETHODIMP get_CanPair(boolean* value) override;
  IFACEMETHODIMP PairAsync(
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Enumeration::DevicePairingResult*>** result)
      override;
  IFACEMETHODIMP PairWithProtectionLevelAsync(
      ABI::Windows::Devices::Enumeration::DevicePairingProtectionLevel
          min_protection_level,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Enumeration::DevicePairingResult*>** result)
      override;

 private:
  bool is_paired_ = false;

  DISALLOW_COPY_AND_ASSIGN(FakeDeviceInformationPairingWinrt);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_FAKE_DEVICE_INFORMATION_PAIRING_WINRT_H_
