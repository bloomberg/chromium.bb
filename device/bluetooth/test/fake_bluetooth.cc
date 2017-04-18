// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/fake_bluetooth.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "device/bluetooth/public/interfaces/test/fake_bluetooth.mojom.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace bluetooth {

FakeBluetooth::FakeBluetooth() {}
FakeBluetooth::~FakeBluetooth() {}

// static
void FakeBluetooth::Create(mojom::FakeBluetoothRequest request) {
  mojo::MakeStrongBinding(base::MakeUnique<FakeBluetooth>(),
                          std::move(request));
}

void FakeBluetooth::SetLESupported(bool available,
                                   const SetLESupportedCallback& callback) {
  // TODO(crbug.com/569709): Actually implement this method.
  callback.Run();
}

}  // namespace bluetooth
