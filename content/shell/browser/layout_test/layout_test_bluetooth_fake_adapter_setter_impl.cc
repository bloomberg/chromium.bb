// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/layout_test/layout_test_bluetooth_fake_adapter_setter_impl.h"

#include <string>

#include "content/public/test/layouttest_support.h"
#include "content/shell/browser/layout_test/layout_test_bluetooth_adapter_provider.h"
#include "device/bluetooth/bluetooth_adapter_factory_wrapper.h"

namespace content {

// static
void LayoutTestBluetoothFakeAdapterSetterImpl::Create(
    mojom::LayoutTestBluetoothFakeAdapterSetterRequest request) {
  new LayoutTestBluetoothFakeAdapterSetterImpl(std::move(request));
}

LayoutTestBluetoothFakeAdapterSetterImpl::
    LayoutTestBluetoothFakeAdapterSetterImpl(
        mojom::LayoutTestBluetoothFakeAdapterSetterRequest request)
    : binding_(this, std::move(request)) {}

LayoutTestBluetoothFakeAdapterSetterImpl::
    ~LayoutTestBluetoothFakeAdapterSetterImpl() {}

void LayoutTestBluetoothFakeAdapterSetterImpl::Set(
    const mojo::String& adapter_name,
    const SetCallback& callback) {

  SetTestBluetoothScanDuration();

  device::BluetoothAdapterFactoryWrapper::Get().SetBluetoothAdapterForTesting(
      LayoutTestBluetoothAdapterProvider::GetBluetoothAdapter(adapter_name));

  callback.Run();
}

}  // namespace content
