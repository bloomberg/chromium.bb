// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/fake_device_watcher_winrt.h"

namespace device {

namespace {

using ABI::Windows::Devices::Enumeration::DeviceInformation;
using ABI::Windows::Devices::Enumeration::DeviceInformationUpdate;
using ABI::Windows::Devices::Enumeration::DeviceWatcher;
using ABI::Windows::Devices::Enumeration::DeviceWatcherStatus;
using ABI::Windows::Foundation::ITypedEventHandler;

}  // namespace

FakeDeviceWatcherWinrt::FakeDeviceWatcherWinrt() = default;

FakeDeviceWatcherWinrt::~FakeDeviceWatcherWinrt() = default;

HRESULT FakeDeviceWatcherWinrt::add_Added(
    ITypedEventHandler<DeviceWatcher*, DeviceInformation*>* handler,
    EventRegistrationToken* token) {
  return E_NOTIMPL;
}

HRESULT FakeDeviceWatcherWinrt::remove_Added(EventRegistrationToken token) {
  return E_NOTIMPL;
}

HRESULT FakeDeviceWatcherWinrt::add_Updated(
    ITypedEventHandler<DeviceWatcher*, DeviceInformationUpdate*>* handler,
    EventRegistrationToken* token) {
  return E_NOTIMPL;
}

HRESULT FakeDeviceWatcherWinrt::remove_Updated(EventRegistrationToken token) {
  return E_NOTIMPL;
}

HRESULT FakeDeviceWatcherWinrt::add_Removed(
    ITypedEventHandler<DeviceWatcher*, DeviceInformationUpdate*>* handler,
    EventRegistrationToken* token) {
  return E_NOTIMPL;
}

HRESULT FakeDeviceWatcherWinrt::remove_Removed(EventRegistrationToken token) {
  return E_NOTIMPL;
}

HRESULT FakeDeviceWatcherWinrt::add_EnumerationCompleted(
    ITypedEventHandler<DeviceWatcher*, IInspectable*>* handler,
    EventRegistrationToken* token) {
  return E_NOTIMPL;
}

HRESULT FakeDeviceWatcherWinrt::remove_EnumerationCompleted(
    EventRegistrationToken token) {
  return E_NOTIMPL;
}

HRESULT FakeDeviceWatcherWinrt::add_Stopped(
    ITypedEventHandler<DeviceWatcher*, IInspectable*>* handler,
    EventRegistrationToken* token) {
  return E_NOTIMPL;
}

HRESULT FakeDeviceWatcherWinrt::remove_Stopped(EventRegistrationToken token) {
  return E_NOTIMPL;
}

HRESULT FakeDeviceWatcherWinrt::get_Status(DeviceWatcherStatus* status) {
  return E_NOTIMPL;
}

HRESULT FakeDeviceWatcherWinrt::Start() {
  return E_NOTIMPL;
}

HRESULT FakeDeviceWatcherWinrt::Stop() {
  return E_NOTIMPL;
}

}  // namespace device
