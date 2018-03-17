// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/multidevice_setup_impl.h"

#include "components/proximity_auth/logging/logging.h"

namespace chromeos {

namespace multidevice {

namespace {

using EventType = multidevice_setup::mojom::EventTypeForDebugging;

std::string EventTypeEnumToString(EventType type) {
  switch (type) {
    case EventType::kNewUserPotentialHostExists:
      return "kNewUserPotentialHostExists";
    case EventType::kExistingUserConnectedHostSwitched:
      return "kExistingUserConnectedHostSwitched";
    case EventType::kExistingUserNewChromebookAdded:
      return "kExistingUserNewChromebookAdded";
    default:
      NOTREACHED();
      return "[invalid input]";
  }
}

}  // namespace

MultiDeviceSetupImpl::MultiDeviceSetupImpl() = default;

MultiDeviceSetupImpl::~MultiDeviceSetupImpl() = default;

void MultiDeviceSetupImpl::BindRequest(
    multidevice_setup::mojom::MultiDeviceSetupRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void MultiDeviceSetupImpl::SetObserver(
    multidevice_setup::mojom::MultiDeviceSetupObserverPtr observer,
    SetObserverCallback callback) {
  if (observer_.is_bound()) {
    PA_LOG(ERROR) << "SetObserver() called when a MultiDeviceSetupObserver was "
                  << "already set. Replacing the previously-set "
                  << "MultiDeviceSetupObserver.";
  }

  PA_LOG(INFO) << "MultiDeviceSetupImpl::SetObserver()";
  observer_ = std::move(observer);
  std::move(callback).Run();
}

void MultiDeviceSetupImpl::TriggerEventForDebugging(
    EventType type,
    TriggerEventForDebuggingCallback callback) {
  PA_LOG(INFO) << "TriggerEventForDebugging(" << EventTypeEnumToString(type)
               << ") called.";

  if (!observer_.is_bound()) {
    PA_LOG(ERROR) << "No MultiDeviceSetupObserver has been set. Cannot "
                  << "proceed.";
    std::move(callback).Run(false /* success */);
    return;
  }

  switch (type) {
    case EventType::kNewUserPotentialHostExists:
      observer_->OnPotentialHostExistsForNewUser();
      break;
    case EventType::kExistingUserConnectedHostSwitched:
      observer_->OnConnectedHostSwitchedForExistingUser();
      break;
    case EventType::kExistingUserNewChromebookAdded:
      observer_->OnNewChromebookAddedForExistingUser();
      break;
    default:
      NOTREACHED();
  }

  std::move(callback).Run(true /* success */);
}

}  // namespace multidevice

}  // namespace chromeos
