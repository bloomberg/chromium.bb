// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <utility>

#include "chromeos/services/multidevice_setup/public/cpp/multidevice_setup_client_impl.h"

#include "base/no_destructor.h"
#include "chromeos/components/proximity_auth/logging/logging.h"
#include "chromeos/services/multidevice_setup/public/mojom/constants.mojom.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace chromeos {

namespace multidevice_setup {

// static
MultiDeviceSetupClientImpl::Factory*
    MultiDeviceSetupClientImpl::Factory::test_factory_ = nullptr;

// static
MultiDeviceSetupClientImpl::Factory*
MultiDeviceSetupClientImpl::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<Factory> factory;
  return factory.get();
}

// static
void MultiDeviceSetupClientImpl::Factory::SetInstanceForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

MultiDeviceSetupClientImpl::Factory::~Factory() = default;

std::unique_ptr<MultiDeviceSetupClient>
MultiDeviceSetupClientImpl::Factory::BuildInstance(
    service_manager::Connector* connector) {
  return base::WrapUnique(new MultiDeviceSetupClientImpl(connector));
}

MultiDeviceSetupClientImpl::MultiDeviceSetupClientImpl(
    service_manager::Connector* connector)
    : host_status_observer_binding_(this),
      feature_state_observer_binding_(this),
      remote_device_cache_(
          cryptauth::RemoteDeviceCache::Factory::Get()->BuildInstance()) {
  connector->BindInterface(mojom::kServiceName, &multidevice_setup_ptr_);
  multidevice_setup_ptr_->AddHostStatusObserver(
      GenerateHostStatusObserverInterfacePtr());
  multidevice_setup_ptr_->AddFeatureStateObserver(
      GenerateFeatureStatesObserverInterfacePtr());
}

MultiDeviceSetupClientImpl::~MultiDeviceSetupClientImpl() = default;

void MultiDeviceSetupClientImpl::GetEligibleHostDevices(
    GetEligibleHostDevicesCallback callback) {
  multidevice_setup_ptr_->GetEligibleHostDevices(base::BindOnce(
      &MultiDeviceSetupClientImpl::OnGetEligibleHostDevicesCompleted,
      base::Unretained(this), std::move(callback)));
}

void MultiDeviceSetupClientImpl::SetHostDevice(
    const std::string& host_device_id,
    const std::string& auth_token,
    mojom::MultiDeviceSetup::SetHostDeviceCallback callback) {
  multidevice_setup_ptr_->SetHostDevice(host_device_id, auth_token,
                                        std::move(callback));
}

void MultiDeviceSetupClientImpl::RemoveHostDevice() {
  multidevice_setup_ptr_->RemoveHostDevice();
}

void MultiDeviceSetupClientImpl::GetHostStatus(GetHostStatusCallback callback) {
  multidevice_setup_ptr_->GetHostStatus(
      base::BindOnce(&MultiDeviceSetupClientImpl::OnGetHostStatusCompleted,
                     base::Unretained(this), std::move(callback)));
}

void MultiDeviceSetupClientImpl::SetFeatureEnabledState(
    mojom::Feature feature,
    bool enabled,
    const base::Optional<std::string>& auth_token,
    mojom::MultiDeviceSetup::SetFeatureEnabledStateCallback callback) {
  multidevice_setup_ptr_->SetFeatureEnabledState(feature, enabled, auth_token,
                                                 std::move(callback));
}

void MultiDeviceSetupClientImpl::GetFeatureStates(
    mojom::MultiDeviceSetup::GetFeatureStatesCallback callback) {
  multidevice_setup_ptr_->GetFeatureStates(std::move(callback));
}

void MultiDeviceSetupClientImpl::RetrySetHostNow(
    mojom::MultiDeviceSetup::RetrySetHostNowCallback callback) {
  multidevice_setup_ptr_->RetrySetHostNow(std::move(callback));
}

void MultiDeviceSetupClientImpl::TriggerEventForDebugging(
    mojom::EventTypeForDebugging type,
    mojom::MultiDeviceSetup::TriggerEventForDebuggingCallback callback) {
  multidevice_setup_ptr_->TriggerEventForDebugging(type, std::move(callback));
}

void MultiDeviceSetupClientImpl::OnHostStatusChanged(
    mojom::HostStatus host_status,
    const base::Optional<cryptauth::RemoteDevice>& host_device) {
  if (host_device) {
    remote_device_cache_->SetRemoteDevices({*host_device});
    NotifyHostStatusChanged(host_status, remote_device_cache_->GetRemoteDevice(
                                             host_device->GetDeviceId()));
  } else {
    NotifyHostStatusChanged(host_status, base::nullopt);
  }
}

void MultiDeviceSetupClientImpl::OnFeatureStatesChanged(
    const FeatureStatesMap& feature_states_map) {
  NotifyFeatureStateChanged(feature_states_map);
}

void MultiDeviceSetupClientImpl::OnGetEligibleHostDevicesCompleted(
    GetEligibleHostDevicesCallback callback,
    const cryptauth::RemoteDeviceList& eligible_host_devices) {
  remote_device_cache_->SetRemoteDevices(eligible_host_devices);

  cryptauth::RemoteDeviceRefList eligible_host_device_refs;
  std::transform(
      eligible_host_devices.begin(), eligible_host_devices.end(),
      std::back_inserter(eligible_host_device_refs),
      [this](const auto& device) {
        return *remote_device_cache_->GetRemoteDevice(device.GetDeviceId());
      });

  std::move(callback).Run(eligible_host_device_refs);
}

void MultiDeviceSetupClientImpl::OnGetHostStatusCompleted(
    GetHostStatusCallback callback,
    mojom::HostStatus host_status,
    const base::Optional<cryptauth::RemoteDevice>& host_device) {
  if (host_device) {
    remote_device_cache_->SetRemoteDevices({*host_device});
    std::move(callback).Run(host_status, *remote_device_cache_->GetRemoteDevice(
                                             host_device->GetDeviceId()));
  } else {
    std::move(callback).Run(host_status, base::nullopt);
  }
}

mojom::HostStatusObserverPtr
MultiDeviceSetupClientImpl::GenerateHostStatusObserverInterfacePtr() {
  mojom::HostStatusObserverPtr interface_ptr;
  host_status_observer_binding_.Bind(mojo::MakeRequest(&interface_ptr));
  return interface_ptr;
}

mojom::FeatureStateObserverPtr
MultiDeviceSetupClientImpl::GenerateFeatureStatesObserverInterfacePtr() {
  mojom::FeatureStateObserverPtr interface_ptr;
  feature_state_observer_binding_.Bind(mojo::MakeRequest(&interface_ptr));
  return interface_ptr;
}

void MultiDeviceSetupClientImpl::FlushForTesting() {
  multidevice_setup_ptr_.FlushForTesting();
}

}  // namespace multidevice_setup

}  // namespace chromeos
