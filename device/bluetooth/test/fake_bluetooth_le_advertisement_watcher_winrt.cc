// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/fake_bluetooth_le_advertisement_watcher_winrt.h"

namespace device {

namespace {

using ABI::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementReceivedEventArgs;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementWatcher;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementWatcherStatus;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementWatcherStoppedEventArgs;
using ABI::Windows::Devices::Bluetooth::Advertisement::BluetoothLEScanningMode;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    IBluetoothLEAdvertisementFilter;
using ABI::Windows::Devices::Bluetooth::IBluetoothSignalStrengthFilter;
using ABI::Windows::Foundation::TimeSpan;
using ABI::Windows::Foundation::ITypedEventHandler;

}  // namespace

FakeBluetoothLEAdvertisementWatcherWinrt::
    FakeBluetoothLEAdvertisementWatcherWinrt() = default;

FakeBluetoothLEAdvertisementWatcherWinrt::
    ~FakeBluetoothLEAdvertisementWatcherWinrt() = default;

HRESULT FakeBluetoothLEAdvertisementWatcherWinrt::get_MinSamplingInterval(
    TimeSpan* value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEAdvertisementWatcherWinrt::get_MaxSamplingInterval(
    TimeSpan* value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEAdvertisementWatcherWinrt::get_MinOutOfRangeTimeout(
    TimeSpan* value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEAdvertisementWatcherWinrt::get_MaxOutOfRangeTimeout(
    TimeSpan* value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEAdvertisementWatcherWinrt::get_Status(
    BluetoothLEAdvertisementWatcherStatus* value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEAdvertisementWatcherWinrt::get_ScanningMode(
    BluetoothLEScanningMode* value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEAdvertisementWatcherWinrt::put_ScanningMode(
    BluetoothLEScanningMode value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEAdvertisementWatcherWinrt::get_SignalStrengthFilter(
    IBluetoothSignalStrengthFilter** value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEAdvertisementWatcherWinrt::put_SignalStrengthFilter(
    IBluetoothSignalStrengthFilter* value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEAdvertisementWatcherWinrt::get_AdvertisementFilter(
    IBluetoothLEAdvertisementFilter** value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEAdvertisementWatcherWinrt::put_AdvertisementFilter(
    IBluetoothLEAdvertisementFilter* value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEAdvertisementWatcherWinrt::Start() {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEAdvertisementWatcherWinrt::Stop() {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEAdvertisementWatcherWinrt::add_Received(
    ITypedEventHandler<BluetoothLEAdvertisementWatcher*,
                       BluetoothLEAdvertisementReceivedEventArgs*>* handler,
    EventRegistrationToken* token) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEAdvertisementWatcherWinrt::remove_Received(
    EventRegistrationToken token) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEAdvertisementWatcherWinrt::add_Stopped(
    ITypedEventHandler<BluetoothLEAdvertisementWatcher*,
                       BluetoothLEAdvertisementWatcherStoppedEventArgs*>*
        handler,
    EventRegistrationToken* token) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEAdvertisementWatcherWinrt::remove_Stopped(
    EventRegistrationToken token) {
  return E_NOTIMPL;
}

}  // namespace device
