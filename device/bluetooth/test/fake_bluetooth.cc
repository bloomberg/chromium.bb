// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/fake_bluetooth.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/public/interfaces/test/fake_bluetooth.mojom.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace bluetooth {

using device::BluetoothAdapterFactory;

FakeBluetooth::FakeBluetooth()
    : global_factory_values_(
          BluetoothAdapterFactory::Get().InitGlobalValuesForTesting()) {}
FakeBluetooth::~FakeBluetooth() {}

// static
void FakeBluetooth::Create(const service_manager::BindSourceInfo& source_info,
                           mojom::FakeBluetoothRequest request) {
  mojo::MakeStrongBinding(base::MakeUnique<FakeBluetooth>(),
                          std::move(request));
}

void FakeBluetooth::SetLESupported(bool supported,
                                   SetLESupportedCallback callback) {
  global_factory_values_->SetLESupported(supported);
  std::move(callback).Run();
}

void FakeBluetooth::SimulateCentral(mojom::CentralState state,
                                    SimulateCentralCallback callback) {
  mojom::FakeCentralPtr fake_central_ptr;
  fake_central_ = base::MakeShared<FakeCentral>(
      state, mojo::MakeRequest(&fake_central_ptr));
  device::BluetoothAdapterFactory::SetAdapterForTesting(fake_central_);
  std::move(callback).Run(std::move(fake_central_ptr));
}

}  // namespace bluetooth
