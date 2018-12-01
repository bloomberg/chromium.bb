// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/eligible_host_devices_provider_impl.h"

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "chromeos/components/multidevice/software_feature_state.h"
#include "components/cryptauth/proto/cryptauth_api.pb.h"

namespace chromeos {

namespace multidevice_setup {

// static
EligibleHostDevicesProviderImpl::Factory*
    EligibleHostDevicesProviderImpl::Factory::test_factory_ = nullptr;

// static
EligibleHostDevicesProviderImpl::Factory*
EligibleHostDevicesProviderImpl::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<Factory> factory;
  return factory.get();
}

// static
void EligibleHostDevicesProviderImpl::Factory::SetFactoryForTesting(
    Factory* factory) {
  test_factory_ = factory;
}

EligibleHostDevicesProviderImpl::Factory::~Factory() = default;

std::unique_ptr<EligibleHostDevicesProvider>
EligibleHostDevicesProviderImpl::Factory::BuildInstance(
    device_sync::DeviceSyncClient* device_sync_client) {
  return base::WrapUnique(
      new EligibleHostDevicesProviderImpl(device_sync_client));
}

EligibleHostDevicesProviderImpl::EligibleHostDevicesProviderImpl(
    device_sync::DeviceSyncClient* device_sync_client)
    : device_sync_client_(device_sync_client) {
  device_sync_client_->AddObserver(this);
  UpdateEligibleDevicesSet();
}

EligibleHostDevicesProviderImpl::~EligibleHostDevicesProviderImpl() {
  device_sync_client_->RemoveObserver(this);
}

multidevice::RemoteDeviceRefList
EligibleHostDevicesProviderImpl::GetEligibleHostDevices() const {
  return eligible_devices_from_last_sync_;
}

void EligibleHostDevicesProviderImpl::OnNewDevicesSynced() {
  UpdateEligibleDevicesSet();
}

void EligibleHostDevicesProviderImpl::UpdateEligibleDevicesSet() {
  eligible_devices_from_last_sync_.clear();
  for (const auto& remote_device : device_sync_client_->GetSyncedDevices()) {
    multidevice::SoftwareFeatureState host_state =
        remote_device.GetSoftwareFeatureState(
            cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST);
    if (host_state == multidevice::SoftwareFeatureState::kSupported ||
        host_state == multidevice::SoftwareFeatureState::kEnabled) {
      eligible_devices_from_last_sync_.push_back(remote_device);
    }
  }
}

}  // namespace multidevice_setup

}  // namespace chromeos
