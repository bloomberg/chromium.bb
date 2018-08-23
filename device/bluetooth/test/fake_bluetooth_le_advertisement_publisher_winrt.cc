// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/fake_bluetooth_le_advertisement_publisher_winrt.h"

namespace device {

namespace {

using ABI::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementPublisher;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementPublisherStatus;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementPublisherStatusChangedEventArgs;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    IBluetoothLEAdvertisement;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    IBluetoothLEAdvertisementPublisher;
using ABI::Windows::Foundation::ITypedEventHandler;

}  // namespace

FakeBluetoothLEAdvertisementPublisherWinrt::
    FakeBluetoothLEAdvertisementPublisherWinrt() = default;

FakeBluetoothLEAdvertisementPublisherWinrt::
    ~FakeBluetoothLEAdvertisementPublisherWinrt() = default;

HRESULT FakeBluetoothLEAdvertisementPublisherWinrt::get_Status(
    BluetoothLEAdvertisementPublisherStatus* value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEAdvertisementPublisherWinrt::get_Advertisement(
    IBluetoothLEAdvertisement** value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEAdvertisementPublisherWinrt::Start() {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEAdvertisementPublisherWinrt::Stop() {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEAdvertisementPublisherWinrt::add_StatusChanged(
    ITypedEventHandler<
        BluetoothLEAdvertisementPublisher*,
        BluetoothLEAdvertisementPublisherStatusChangedEventArgs*>* handler,
    EventRegistrationToken* token) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEAdvertisementPublisherWinrt::remove_StatusChanged(
    EventRegistrationToken token) {
  return E_NOTIMPL;
}

FakeBluetoothLEAdvertisementPublisherFactoryWinrt::
    FakeBluetoothLEAdvertisementPublisherFactoryWinrt() = default;

FakeBluetoothLEAdvertisementPublisherFactoryWinrt::
    ~FakeBluetoothLEAdvertisementPublisherFactoryWinrt() = default;

HRESULT FakeBluetoothLEAdvertisementPublisherFactoryWinrt::Create(
    IBluetoothLEAdvertisement* advertisement,
    IBluetoothLEAdvertisementPublisher** value) {
  return E_NOTIMPL;
}

}  // namespace device
