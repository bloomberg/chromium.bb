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
#include "chromeos/services/multidevice_setup/feature_state_manager_impl.h"
#include "chromeos/services/multidevice_setup/host_backend_delegate_impl.h"
#include "chromeos/services/multidevice_setup/host_status_provider_impl.h"
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
    secure_channel::SecureChannelClient* secure_channel_client,
    AuthTokenValidator* auth_token_validator) {
  return base::WrapUnique(
      new MultiDeviceSetupImpl(pref_service, device_sync_client,
                               secure_channel_client, auth_token_validator));
}

MultiDeviceSetupImpl::MultiDeviceSetupImpl(
    PrefService* pref_service,
    device_sync::DeviceSyncClient* device_sync_client,
    secure_channel::SecureChannelClient* secure_channel_client,
    AuthTokenValidator* auth_token_validator)
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
          pref_service)),
      host_status_provider_(
          HostStatusProviderImpl::Factory::Get()->BuildInstance(
              eligible_host_devices_provider_.get(),
              host_backend_delegate_.get(),
              host_verifier_.get(),
              device_sync_client)),
      feature_state_manager_(
          FeatureStateManagerImpl::Factory::Get()->BuildInstance(
              pref_service,
              host_status_provider_.get(),
              device_sync_client)),
      setup_flow_completion_recorder_(
          SetupFlowCompletionRecorderImpl::Factory::Get()->BuildInstance(
              pref_service,
              base::DefaultClock::GetInstance())),
      delegate_notifier_(
          AccountStatusChangeDelegateNotifierImpl::Factory::Get()
              ->BuildInstance(device_sync_client,
                              pref_service,
                              setup_flow_completion_recorder_.get(),
                              base::DefaultClock::GetInstance())) {
  host_status_provider_->AddObserver(this);
  feature_state_manager_->AddObserver(this);
}

MultiDeviceSetupImpl::~MultiDeviceSetupImpl() {
  host_status_provider_->RemoveObserver(this);
  feature_state_manager_->RemoveObserver(this);
}

void MultiDeviceSetupImpl::SetAccountStatusChangeDelegate(
    mojom::AccountStatusChangeDelegatePtr delegate) {
  delegate_notifier_->SetAccountStatusChangeDelegatePtr(std::move(delegate));
}

void MultiDeviceSetupImpl::AddHostStatusObserver(
    mojom::HostStatusObserverPtr observer) {
  host_status_observers_.AddPtr(std::move(observer));
}

void MultiDeviceSetupImpl::AddFeatureStateObserver(
    mojom::FeatureStateObserverPtr observer) {
  feature_state_observers_.AddPtr(std::move(observer));
}

void MultiDeviceSetupImpl::GetEligibleHostDevices(
    GetEligibleHostDevicesCallback callback) {
  std::vector<cryptauth::RemoteDevice> eligible_remote_devices;
  for (const auto& remote_device_ref :
       eligible_host_devices_provider_->GetEligibleHostDevices()) {
    eligible_remote_devices.push_back(remote_device_ref.GetRemoteDevice());
  }

  std::move(callback).Run(eligible_remote_devices);
}

void MultiDeviceSetupImpl::SetHostDevice(const std::string& host_device_id,
                                         SetHostDeviceCallback callback) {
  // TODO(crbug.com/870122): Use AuthTokenValidator to verify that the
  // user is authenticated.

  cryptauth::RemoteDeviceRefList eligible_devices =
      eligible_host_devices_provider_->GetEligibleHostDevices();
  auto it =
      std::find_if(eligible_devices.begin(), eligible_devices.end(),
                   [&host_device_id](const auto& eligible_device) {
                     return eligible_device.GetDeviceId() == host_device_id;
                   });

  if (it == eligible_devices.end()) {
    std::move(callback).Run(false /* success */);
    return;
  }

  host_backend_delegate_->AttemptToSetMultiDeviceHostOnBackend(*it);
  std::move(callback).Run(true /* success */);
}

void MultiDeviceSetupImpl::RemoveHostDevice() {
  host_backend_delegate_->AttemptToSetMultiDeviceHostOnBackend(
      base::nullopt /* host_device */);
}

void MultiDeviceSetupImpl::GetHostStatus(GetHostStatusCallback callback) {
  HostStatusProvider::HostStatusWithDevice host_status_with_device =
      host_status_provider_->GetHostWithStatus();

  // The Mojo API requires a raw cryptauth::RemoteDevice instead of a
  // cryptauth::RemoteDeviceRef.
  base::Optional<cryptauth::RemoteDevice> device_for_callback;
  if (host_status_with_device.host_device()) {
    device_for_callback =
        host_status_with_device.host_device()->GetRemoteDevice();
  }

  std::move(callback).Run(host_status_with_device.host_status(),
                          device_for_callback);
}

void MultiDeviceSetupImpl::SetFeatureEnabledState(
    mojom::Feature feature,
    bool enabled,
    SetFeatureEnabledStateCallback callback) {
  std::move(callback).Run(
      feature_state_manager_->SetFeatureEnabledState(feature, enabled));
}

void MultiDeviceSetupImpl::GetFeatureStates(GetFeatureStatesCallback callback) {
  std::move(callback).Run(feature_state_manager_->GetFeatureStates());
}

void MultiDeviceSetupImpl::RetrySetHostNow(RetrySetHostNowCallback callback) {
  HostStatusProvider::HostStatusWithDevice host_status_with_device =
      host_status_provider_->GetHostWithStatus();

  if (host_status_with_device.host_status() ==
      mojom::HostStatus::kHostSetLocallyButWaitingForBackendConfirmation) {
    host_backend_delegate_->AttemptToSetMultiDeviceHostOnBackend(
        *host_backend_delegate_->GetPendingHostRequest());
    std::move(callback).Run(true /* success */);
    return;
  }

  if (host_status_with_device.host_status() ==
      mojom::HostStatus::kHostSetButNotYetVerified) {
    host_verifier_->AttemptVerificationNow();
    std::move(callback).Run(true /* success */);
    return;
  }

  // RetrySetHostNow() was called when there was nothing to retry.
  std::move(callback).Run(false /* success */);
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

void MultiDeviceSetupImpl::OnHostStatusChange(
    const HostStatusProvider::HostStatusWithDevice& host_status_with_device) {
  mojom::HostStatus status_for_callback = host_status_with_device.host_status();

  // The Mojo API requires a raw cryptauth::RemoteDevice instead of a
  // cryptauth::RemoteDeviceRef.
  base::Optional<cryptauth::RemoteDevice> device_for_callback;
  if (host_status_with_device.host_device()) {
    device_for_callback =
        host_status_with_device.host_device()->GetRemoteDevice();
  }

  host_status_observers_.ForAllPtrs(
      [&status_for_callback,
       &device_for_callback](mojom::HostStatusObserver* observer) {
        observer->OnHostStatusChanged(status_for_callback, device_for_callback);
      });
}

void MultiDeviceSetupImpl::OnFeatureStatesChange(
    const FeatureStateManager::FeatureStatesMap& feature_states_map) {
  feature_state_observers_.ForAllPtrs(
      [&feature_states_map](mojom::FeatureStateObserver* observer) {
        observer->OnFeatureStatesChanged(feature_states_map);
      });
}

void MultiDeviceSetupImpl::FlushForTesting() {
  host_status_observers_.FlushForTesting();
  feature_state_observers_.FlushForTesting();
}

}  // namespace multidevice_setup

}  // namespace chromeos
