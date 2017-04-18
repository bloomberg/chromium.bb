// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_BLUETOOTH_TEST_FAKE_BLUETOOTH_H_
#define DEVICE_BLUETOOTH_TEST_FAKE_BLUETOOTH_H_

#include "base/compiler_specific.h"
#include "device/bluetooth/public/interfaces/test/fake_bluetooth.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace bluetooth {

// Implementation of FakeBluetooth in
// src/device/bluetooth/public/interfaces/test/fake_bluetooth.mojom.
// Implemented on top of the C++ device/bluetooth API.
class FakeBluetooth : NON_EXPORTED_BASE(public mojom::FakeBluetooth) {
 public:
  FakeBluetooth();
  ~FakeBluetooth() override;

  static void Create(mojom::FakeBluetoothRequest request);

  void SetLESupported(bool available,
                      const SetLESupportedCallback& callback) override;
};

}  // namespace bluetooth

#endif  // DEVICE_BLUETOOTH_TEST_FAKE_BLUETOOTH_H_
