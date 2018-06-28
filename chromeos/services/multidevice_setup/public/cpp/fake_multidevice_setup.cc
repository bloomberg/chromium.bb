// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/public/cpp/fake_multidevice_setup.h"

namespace chromeos {

namespace multidevice_setup {

FakeMultiDeviceSetup::FakeMultiDeviceSetup() = default;

FakeMultiDeviceSetup::~FakeMultiDeviceSetup() = default;

void FakeMultiDeviceSetup::BindRequest(mojom::MultiDeviceSetupRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void FakeMultiDeviceSetup::BindHandle(mojo::ScopedMessagePipeHandle handle) {
  BindRequest(chromeos::multidevice_setup::mojom::MultiDeviceSetupRequest(
      std::move(handle)));
}

void FakeMultiDeviceSetup::SetAccountStatusChangeDelegate(
    mojom::AccountStatusChangeDelegatePtr delegate) {
  delegate_ = std::move(delegate);
}

void FakeMultiDeviceSetup::TriggerEventForDebugging(
    mojom::EventTypeForDebugging type,
    TriggerEventForDebuggingCallback callback) {
  triggered_debug_events_.emplace_back(type, std::move(callback));
}

}  // namespace multidevice_setup

}  // namespace chromeos
