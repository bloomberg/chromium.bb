// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/tether_host_fetcher_impl.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "chromeos/components/multidevice/remote_device.h"

namespace chromeos {

namespace tether {

// static
TetherHostFetcherImpl::Factory*
    TetherHostFetcherImpl::Factory::factory_instance_ = nullptr;

// static
std::unique_ptr<TetherHostFetcher> TetherHostFetcherImpl::Factory::NewInstance(
    device_sync::DeviceSyncClient* device_sync_client,
    chromeos::multidevice_setup::MultiDeviceSetupClient*
        multidevice_setup_client) {
  if (!factory_instance_) {
    factory_instance_ = new Factory();
  }
  return factory_instance_->BuildInstance(device_sync_client,
                                          multidevice_setup_client);
}

// static
void TetherHostFetcherImpl::Factory::SetInstanceForTesting(Factory* factory) {
  factory_instance_ = factory;
}

std::unique_ptr<TetherHostFetcher>
TetherHostFetcherImpl::Factory::BuildInstance(
    device_sync::DeviceSyncClient* device_sync_client,
    chromeos::multidevice_setup::MultiDeviceSetupClient*
        multidevice_setup_client) {
  return base::WrapUnique(
      new TetherHostFetcherImpl(device_sync_client, multidevice_setup_client));
}

TetherHostFetcherImpl::TetherHostFetcherImpl(
    device_sync::DeviceSyncClient* device_sync_client,
    chromeos::multidevice_setup::MultiDeviceSetupClient*
        multidevice_setup_client)
    : device_sync_client_(device_sync_client),
      multidevice_setup_client_(multidevice_setup_client),
      weak_ptr_factory_(this) {
  device_sync_client_->AddObserver(this);
  multidevice_setup_client_->AddObserver(this);

  CacheCurrentTetherHosts();
}

TetherHostFetcherImpl::~TetherHostFetcherImpl() {
  device_sync_client_->RemoveObserver(this);
  multidevice_setup_client_->RemoveObserver(this);
}

bool TetherHostFetcherImpl::HasSyncedTetherHosts() {
  return !current_remote_device_list_.empty();
}

void TetherHostFetcherImpl::FetchAllTetherHosts(
    const TetherHostListCallback& callback) {
  ProcessFetchAllTetherHostsRequest(current_remote_device_list_, callback);
}

void TetherHostFetcherImpl::FetchTetherHost(
    const std::string& device_id,
    const TetherHostCallback& callback) {
  ProcessFetchSingleTetherHostRequest(device_id, current_remote_device_list_,
                                      callback);
}

void TetherHostFetcherImpl::OnNewDevicesSynced() {
  CacheCurrentTetherHosts();
}

void TetherHostFetcherImpl::OnHostStatusChanged(
    const multidevice_setup::MultiDeviceSetupClient::HostStatusWithDevice&
        host_status_with_device) {
  CacheCurrentTetherHosts();
}

void TetherHostFetcherImpl::OnFeatureStatesChanged(
    const multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
        feature_states_map) {
  CacheCurrentTetherHosts();
}

void TetherHostFetcherImpl::OnReady() {
  CacheCurrentTetherHosts();
}

void TetherHostFetcherImpl::CacheCurrentTetherHosts() {
  multidevice::RemoteDeviceRefList updated_list = GenerateHostDeviceList();
  if (updated_list == current_remote_device_list_)
    return;

  current_remote_device_list_.swap(updated_list);
  NotifyTetherHostsUpdated();
}

multidevice::RemoteDeviceRefList
TetherHostFetcherImpl::GenerateHostDeviceList() {
  multidevice::RemoteDeviceRefList host_list;

  TetherHostSource tether_host_source =
      IsInLegacyHostMode() ? TetherHostSource::DEVICE_SYNC_CLIENT
                           : TetherHostSource::MULTIDEVICE_SETUP_CLIENT;

  if (tether_host_source == TetherHostSource::MULTIDEVICE_SETUP_CLIENT) {
    multidevice_setup::MultiDeviceSetupClient::HostStatusWithDevice
        host_status_with_device = multidevice_setup_client_->GetHostStatus();
    if (host_status_with_device.first ==
        chromeos::multidevice_setup::mojom::HostStatus::kHostVerified) {
      host_list.push_back(*host_status_with_device.second);
    }
    return host_list;
  }

  if (tether_host_source == TetherHostSource::DEVICE_SYNC_CLIENT) {
    for (const multidevice::RemoteDeviceRef& remote_device_ref :
         device_sync_client_->GetSyncedDevices()) {
      multidevice::SoftwareFeatureState magic_tether_host_state =
          remote_device_ref.GetSoftwareFeatureState(
              chromeos::multidevice::SoftwareFeature::kInstantTetheringHost);
      if (magic_tether_host_state ==
              multidevice::SoftwareFeatureState::kSupported ||
          magic_tether_host_state ==
              multidevice::SoftwareFeatureState::kEnabled) {
        host_list.push_back(remote_device_ref);
      }
    }
    return host_list;
  }

  NOTREACHED();
  return host_list;
}

bool TetherHostFetcherImpl::IsInLegacyHostMode() {
  if (!device_sync_client_->is_ready())
    return false;

  bool has_supported_tether_host = false;
  for (const multidevice::RemoteDeviceRef& remote_device_ref :
       device_sync_client_->GetSyncedDevices()) {
    multidevice::SoftwareFeatureState better_together_host_state =
        remote_device_ref.GetSoftwareFeatureState(
            chromeos::multidevice::SoftwareFeature::kBetterTogetherHost);
    // If there's any valid Better Together host, don't support legacy mode.
    if (better_together_host_state ==
            multidevice::SoftwareFeatureState::kSupported ||
        better_together_host_state ==
            multidevice::SoftwareFeatureState::kEnabled) {
      return false;
    }

    multidevice::SoftwareFeatureState magic_tether_host_state =
        remote_device_ref.GetSoftwareFeatureState(
            chromeos::multidevice::SoftwareFeature::kInstantTetheringHost);
    if (magic_tether_host_state ==
            multidevice::SoftwareFeatureState::kSupported ||
        magic_tether_host_state ==
            multidevice::SoftwareFeatureState::kEnabled) {
      has_supported_tether_host = true;
    }
  }

  return has_supported_tether_host;
}

}  // namespace tether

}  // namespace chromeos
