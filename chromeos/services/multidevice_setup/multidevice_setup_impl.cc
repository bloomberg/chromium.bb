// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/multidevice_setup_impl.h"

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/time/default_clock.h"
#include "chromeos/components/proximity_auth/logging/logging.h"
#include "chromeos/services/multidevice_setup/account_status_change_delegate_notifier_impl.h"
#include "chromeos/services/multidevice_setup/eligible_host_devices_provider_impl.h"
#include "chromeos/services/multidevice_setup/host_backend_delegate_impl.h"
#include "chromeos/services/multidevice_setup/host_verifier_impl.h"
#include "chromeos/services/multidevice_setup/setup_flow_completion_recorder_impl.h"

namespace chromeos {

namespace multidevice_setup {

// static
MultiDeviceSetupImpl::Factory* MultiDeviceSetupImpl::Factory::test_factory_ =
    nullptr;

// static
MultiDeviceSetupImpl::Factory* MultiDeviceSetupImpl::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<Factory> factory;
  return factory.get();
}

// static
void MultiDeviceSetupImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

MultiDeviceSetupImpl::Factory::~Factory() = default;

std::unique_ptr<mojom::MultiDeviceSetup>
MultiDeviceSetupImpl::Factory::BuildInstance(
    PrefService* pref_service,
    device_sync::DeviceSyncClient* device_sync_client,
    secure_channel::SecureChannelClient* secure_channel_client) {
  return base::WrapUnique(new MultiDeviceSetupImpl(
      pref_service, device_sync_client, secure_channel_client));
}

MultiDeviceSetupImpl::MultiDeviceSetupImpl(
    PrefService* pref_service,
    device_sync::DeviceSyncClient* device_sync_client,
    secure_channel::SecureChannelClient* secure_channel_client)
    : eligible_host_devices_provider_(
          EligibleHostDevicesProviderImpl::Factory::Get()->BuildInstance(
              device_sync_client)),
      host_backend_delegate_(
          HostBackendDelegateImpl::Factory::Get()->BuildInstance(
              eligible_host_devices_provider_.get(),
              pref_service,
              device_sync_client)),
      host_verifier_(HostVerifierImpl::Factory::Get()->BuildInstance(
          host_backend_delegate_.get(),
          device_sync_client,
          secure_channel_client)),
      setup_flow_completion_recorder_(
          SetupFlowCompletionRecorderImpl::Factory::Get()->BuildInstance(
              pref_service,
              base::DefaultClock::GetInstance())),
      delegate_notifier_(
          AccountStatusChangeDelegateNotifierImpl::Factory::Get()
              ->BuildInstance(device_sync_client,
                              pref_service,
                              setup_flow_completion_recorder_.get(),
                              base::DefaultClock::GetInstance())) {}

MultiDeviceSetupImpl::~MultiDeviceSetupImpl() = default;

void MultiDeviceSetupImpl::SetAccountStatusChangeDelegate(
    mojom::AccountStatusChangeDelegatePtr delegate) {
  delegate_notifier_->SetAccountStatusChangeDelegatePtr(std::move(delegate));
}

void MultiDeviceSetupImpl::TriggerEventForDebugging(
    mojom::EventTypeForDebugging type,
    TriggerEventForDebuggingCallback callback) {
  if (!delegate_notifier_->delegate_ptr_) {
    PA_LOG(ERROR) << "MultiDeviceSetupImpl::TriggerEventForDebugging(): No "
                  << "delgate has been set; cannot proceed.";
    std::move(callback).Run(false /* success */);
    return;
  }

  PA_LOG(INFO) << "MultiDeviceSetupImpl::TriggerEventForDebugging(" << type
               << ") called.";
  mojom::AccountStatusChangeDelegate* delegate =
      delegate_notifier_->delegate_ptr_.get();

  switch (type) {
    case mojom::EventTypeForDebugging::kNewUserPotentialHostExists:
      delegate->OnPotentialHostExistsForNewUser();
      break;
    case mojom::EventTypeForDebugging::kExistingUserConnectedHostSwitched:
      delegate->OnConnectedHostSwitchedForExistingUser();
      break;
    case mojom::EventTypeForDebugging::kExistingUserNewChromebookAdded:
      delegate->OnNewChromebookAddedForExistingUser();
      break;
    default:
      NOTREACHED();
  }

  std::move(callback).Run(true /* success */);
}

}  // namespace multidevice_setup

}  // namespace chromeos
