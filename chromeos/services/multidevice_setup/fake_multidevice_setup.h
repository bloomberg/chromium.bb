// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_FAKE_MULTIDEVICE_SETUP_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_FAKE_MULTIDEVICE_SETUP_H_

#include <utility>
#include <vector>

#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"

namespace chromeos {

namespace multidevice_setup {

// Test MultiDeviceSetup implementation.
class FakeMultiDeviceSetup : public mojom::MultiDeviceSetup {
 public:
  FakeMultiDeviceSetup();
  ~FakeMultiDeviceSetup() override;

  mojom::AccountStatusChangeDelegatePtr& delegate() { return delegate_; }

  std::vector<std::pair<mojom::EventTypeForDebugging,
                        TriggerEventForDebuggingCallback>>&
  triggered_debug_events() {
    return triggered_debug_events_;
  }

 private:
  // mojom::MultiDeviceSetup:
  void SetAccountStatusChangeDelegate(
      mojom::AccountStatusChangeDelegatePtr delegate,
      SetAccountStatusChangeDelegateCallback callback) override;
  void TriggerEventForDebugging(
      mojom::EventTypeForDebugging type,
      TriggerEventForDebuggingCallback callback) override;

  mojom::AccountStatusChangeDelegatePtr delegate_;

  std::vector<
      std::pair<mojom::EventTypeForDebugging, TriggerEventForDebuggingCallback>>
      triggered_debug_events_;

  DISALLOW_COPY_AND_ASSIGN(FakeMultiDeviceSetup);
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_FAKE_MULTIDEVICE_SETUP_H_
