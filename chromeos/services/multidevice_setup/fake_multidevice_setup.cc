// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/fake_multidevice_setup.h"

namespace chromeos {

namespace multidevice_setup {

FakeMultiDeviceSetup::FakeMultiDeviceSetup() = default;

FakeMultiDeviceSetup::~FakeMultiDeviceSetup() = default;

void FakeMultiDeviceSetup::SetAccountStatusChangeDelegate(
    mojom::AccountStatusChangeDelegatePtr delegate,
    SetAccountStatusChangeDelegateCallback callback) {
  delegate_ = std::move(delegate);
  std::move(callback).Run();
}

void FakeMultiDeviceSetup::TriggerEventForDebugging(
    mojom::EventTypeForDebugging type,
    TriggerEventForDebuggingCallback callback) {
  triggered_debug_events_.emplace_back(type, std::move(callback));
}

}  // namespace multidevice_setup

}  // namespace chromeos
