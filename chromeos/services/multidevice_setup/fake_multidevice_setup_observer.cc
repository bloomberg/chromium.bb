// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/fake_multidevice_setup_observer.h"

namespace chromeos {

namespace multidevice {

FakeMultiDeviceSetupObserver::FakeMultiDeviceSetupObserver() = default;

FakeMultiDeviceSetupObserver::~FakeMultiDeviceSetupObserver() = default;

multidevice_setup::mojom::MultiDeviceSetupObserverPtr
FakeMultiDeviceSetupObserver::GenerateInterfacePtr() {
  multidevice_setup::mojom::MultiDeviceSetupObserverPtr interface_ptr;
  bindings_.AddBinding(this, mojo::MakeRequest(&interface_ptr));
  return interface_ptr;
}

void FakeMultiDeviceSetupObserver::OnPotentialHostExistsForNewUser() {
  ++num_new_user_events_handled_;
}

void FakeMultiDeviceSetupObserver::OnConnectedHostSwitchedForExistingUser() {
  ++num_existing_user_host_switched_events_handled_;
}

void FakeMultiDeviceSetupObserver::OnNewChromebookAddedForExistingUser() {
  ++num_existing_user_chromebook_added_events_handled_;
}

}  // namespace multidevice

}  // namespace chromeos
