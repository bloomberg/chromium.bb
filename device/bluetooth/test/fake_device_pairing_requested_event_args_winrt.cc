// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/fake_device_pairing_requested_event_args_winrt.h"

#include <windows.foundation.h>

namespace device {

namespace {

using ABI::Windows::Devices::Enumeration::IDeviceInformation;
using ABI::Windows::Devices::Enumeration::DevicePairingKinds;
using ABI::Windows::Foundation::IDeferral;

class FakeDeferral
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::Foundation::IDeferral> {
 public:
  FakeDeferral() = default;
  ~FakeDeferral() override = default;

  // IDeferral:
  IFACEMETHODIMP Complete() override { return E_NOTIMPL; }

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeDeferral);
};

}  // namespace

FakeDevicePairingRequestedEventArgsWinrt::
    FakeDevicePairingRequestedEventArgsWinrt() = default;

FakeDevicePairingRequestedEventArgsWinrt::
    ~FakeDevicePairingRequestedEventArgsWinrt() = default;

HRESULT FakeDevicePairingRequestedEventArgsWinrt::get_DeviceInformation(
    IDeviceInformation** value) {
  return E_NOTIMPL;
}

HRESULT FakeDevicePairingRequestedEventArgsWinrt::get_PairingKind(
    DevicePairingKinds* value) {
  return E_NOTIMPL;
}

HRESULT FakeDevicePairingRequestedEventArgsWinrt::get_Pin(HSTRING* value) {
  return E_NOTIMPL;
}

HRESULT FakeDevicePairingRequestedEventArgsWinrt::Accept() {
  return E_NOTIMPL;
}

HRESULT FakeDevicePairingRequestedEventArgsWinrt::AcceptWithPin(HSTRING pin) {
  return E_NOTIMPL;
}

HRESULT FakeDevicePairingRequestedEventArgsWinrt::GetDeferral(
    IDeferral** result) {
  return E_NOTIMPL;
}

}  // namespace device
