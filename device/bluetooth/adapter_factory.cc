// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/memory/ptr_util.h"
#include "device/bluetooth/adapter.h"
#include "device/bluetooth/adapter_factory.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace bluetooth {

AdapterFactory::AdapterFactory() : weak_ptr_factory_(this) {}
AdapterFactory::~AdapterFactory() {}

// static
void AdapterFactory::Create(const service_manager::BindSourceInfo& source_info,
                            mojom::AdapterFactoryRequest request) {
  mojo::MakeStrongBinding(base::MakeUnique<AdapterFactory>(),
                          std::move(request));
}

void AdapterFactory::GetAdapter(GetAdapterCallback callback) {
  if (device::BluetoothAdapterFactory::IsBluetoothSupported()) {
    device::BluetoothAdapterFactory::GetAdapter(
        base::Bind(&AdapterFactory::OnGetAdapter,
                   weak_ptr_factory_.GetWeakPtr(), base::Passed(&callback)));
  } else {
    std::move(callback).Run(nullptr /* AdapterPtr */);
  }
}

void AdapterFactory::OnGetAdapter(
    GetAdapterCallback callback,
    scoped_refptr<device::BluetoothAdapter> adapter) {
  mojom::AdapterPtr adapter_ptr;
  mojo::MakeStrongBinding(base::MakeUnique<Adapter>(adapter),
                          mojo::MakeRequest(&adapter_ptr));
  std::move(callback).Run(std::move(adapter_ptr));
}

}  // namespace bluetooth
