// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/fake_gatt_read_result_winrt.h"

#include <wrl/client.h>

#include <utility>

#include "base/win/winrt_storage_util.h"

namespace {

using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCommunicationStatus;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCommunicationStatus_Success;
using ABI::Windows::Storage::Streams::IBuffer;
using Microsoft::WRL::ComPtr;
}  // namespace

namespace device {

FakeGattReadResultWinrt::FakeGattReadResultWinrt(GattCommunicationStatus status)
    : status_(status) {}

FakeGattReadResultWinrt::FakeGattReadResultWinrt(std::vector<uint8_t> data)
    : status_(GattCommunicationStatus_Success), data_(std::move(data)) {}

FakeGattReadResultWinrt::~FakeGattReadResultWinrt() = default;

HRESULT FakeGattReadResultWinrt::get_Status(GattCommunicationStatus* value) {
  *value = status_;
  return S_OK;
}

HRESULT FakeGattReadResultWinrt::get_Value(IBuffer** value) {
  ComPtr<IBuffer> buffer;
  HRESULT hr =
      base::win::CreateIBufferFromData(data_.data(), data_.size(), &buffer);
  if (FAILED(hr))
    return hr;

  return buffer.CopyTo(value);
}

}  // namespace device
