// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/layout_test/layout_test_bluetooth_fake_adapter_setter_impl.h"

#include <string>
#include <utility>

#include "base/memory/ptr_util.h"
#include "content/public/test/layouttest_support.h"
#include "content/shell/browser/layout_test/layout_test_bluetooth_adapter_provider.h"
#include "device/bluetooth/bluetooth_adapter_factory_wrapper.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace content {

LayoutTestBluetoothFakeAdapterSetterImpl::
    LayoutTestBluetoothFakeAdapterSetterImpl() {}

LayoutTestBluetoothFakeAdapterSetterImpl::
    ~LayoutTestBluetoothFakeAdapterSetterImpl() {}

// static
void LayoutTestBluetoothFakeAdapterSetterImpl::Create(
    const service_manager::BindSourceInfo& source_info,
    mojom::LayoutTestBluetoothFakeAdapterSetterRequest request) {
  mojo::MakeStrongBinding(
      base::MakeUnique<LayoutTestBluetoothFakeAdapterSetterImpl>(),
      std::move(request));
}

void LayoutTestBluetoothFakeAdapterSetterImpl::Set(
    const std::string& adapter_name,
    SetCallback callback) {
  SetTestBluetoothScanDuration();

  device::BluetoothAdapterFactoryWrapper::Get().SetBluetoothAdapterForTesting(
      LayoutTestBluetoothAdapterProvider::GetBluetoothAdapter(adapter_name));

  std::move(callback).Run();
}

}  // namespace content
