// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/multidevice_setup_impl.h"

#include "chromeos/components/proximity_auth/logging/logging.h"

namespace chromeos {

namespace multidevice_setup {

MultiDeviceSetupImpl::MultiDeviceSetupImpl() = default;

MultiDeviceSetupImpl::~MultiDeviceSetupImpl() = default;

void MultiDeviceSetupImpl::BindRequest(mojom::MultiDeviceSetupRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void MultiDeviceSetupImpl::SetAccountStatusChangeDelegate(
    mojom::AccountStatusChangeDelegatePtr delegate,
    SetAccountStatusChangeDelegateCallback callback) {
  delegate_ = std::move(delegate);
  std::move(callback).Run();
}

void MultiDeviceSetupImpl::TriggerEventForDebugging(
    mojom::EventTypeForDebugging type,
    TriggerEventForDebuggingCallback callback) {
  PA_LOG(INFO) << "MultiDeviceSetupImpl::TriggerEventForDebugging(" << type
               << ") called.";

  if (!delegate_.is_bound()) {
    PA_LOG(ERROR) << "MultiDeviceSetupImpl::TriggerEventForDebugging(): No "
                  << "delgate has been set; cannot proceed.";
    std::move(callback).Run(false /* success */);
    return;
  }

  switch (type) {
    case mojom::EventTypeForDebugging::kNewUserPotentialHostExists:
      delegate_->OnPotentialHostExistsForNewUser();
      break;
    case mojom::EventTypeForDebugging::kExistingUserConnectedHostSwitched:
      delegate_->OnConnectedHostSwitchedForExistingUser();
      break;
    case mojom::EventTypeForDebugging::kExistingUserNewChromebookAdded:
      delegate_->OnNewChromebookAddedForExistingUser();
      break;
    default:
      NOTREACHED();
  }

  std::move(callback).Run(true /* success */);
}

}  // namespace multidevice_setup

}  // namespace chromeos
