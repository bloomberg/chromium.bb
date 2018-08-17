// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_FAKE_DEVICE_PAIRING_REQUESTED_EVENT_ARGS_WINRT_H_
#define DEVICE_BLUETOOTH_TEST_FAKE_DEVICE_PAIRING_REQUESTED_EVENT_ARGS_WINRT_H_

#include <windows.devices.enumeration.h>
#include <wrl/implements.h>

#include "base/macros.h"

namespace device {

class FakeDevicePairingRequestedEventArgsWinrt
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::Devices::Enumeration::
              IDevicePairingRequestedEventArgs> {
 public:
  FakeDevicePairingRequestedEventArgsWinrt();
  ~FakeDevicePairingRequestedEventArgsWinrt() override;

  // IDevicePairingRequestedEventArgs:
  IFACEMETHODIMP get_DeviceInformation(
      ABI::Windows::Devices::Enumeration::IDeviceInformation** value) override;
  IFACEMETHODIMP get_PairingKind(
      ABI::Windows::Devices::Enumeration::DevicePairingKinds* value) override;
  IFACEMETHODIMP get_Pin(HSTRING* value) override;
  IFACEMETHODIMP Accept() override;
  IFACEMETHODIMP AcceptWithPin(HSTRING pin) override;
  IFACEMETHODIMP GetDeferral(
      ABI::Windows::Foundation::IDeferral** result) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeDevicePairingRequestedEventArgsWinrt);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_FAKE_DEVICE_PAIRING_REQUESTED_EVENT_ARGS_WINRT_H_
