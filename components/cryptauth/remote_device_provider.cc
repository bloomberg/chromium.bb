// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cryptauth/remote_device_provider.h"

#include "base/bind.h"
#include "components/cryptauth/remote_device_loader.h"
#include "components/cryptauth/secure_message_delegate.h"

namespace cryptauth {

RemoteDeviceProvider::RemoteDeviceProvider(
    CryptAuthDeviceManager* device_manager,
    const std::string& user_id,
    const std::string& user_private_key,
    SecureMessageDelegateFactory* secure_message_delegate_factory)
    : device_manager_(device_manager),
      user_id_(user_id),
      user_private_key_(user_private_key),
      secure_message_delegate_factory_(secure_message_delegate_factory),
      weak_ptr_factory_(this) {
  device_manager_->AddObserver(this);
  remote_device_loader_ = cryptauth::RemoteDeviceLoader::Factory::NewInstance(
      device_manager->GetSyncedDevices(), user_id, user_private_key,
      secure_message_delegate_factory->CreateSecureMessageDelegate());
  remote_device_loader_->Load(
      false /* should_load_beacon_seeds */,
      base::Bind(&RemoteDeviceProvider::OnRemoteDevicesLoaded,
                 weak_ptr_factory_.GetWeakPtr()));
}

RemoteDeviceProvider::~RemoteDeviceProvider() {
  device_manager_->RemoveObserver(this);
}

void RemoteDeviceProvider::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void RemoteDeviceProvider::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void RemoteDeviceProvider::OnSyncFinished(
    CryptAuthDeviceManager::SyncResult sync_result,
    CryptAuthDeviceManager::DeviceChangeResult device_change_result) {
  if (sync_result == CryptAuthDeviceManager::SyncResult::SUCCESS &&
      device_change_result ==
          CryptAuthDeviceManager::DeviceChangeResult::CHANGED) {
    remote_device_loader_ = cryptauth::RemoteDeviceLoader::Factory::NewInstance(
        device_manager_->GetSyncedDevices(), user_id_, user_private_key_,
        secure_message_delegate_factory_->CreateSecureMessageDelegate());

    remote_device_loader_->Load(
        false /* should_load_beacon_seeds */,
        base::Bind(&RemoteDeviceProvider::OnRemoteDevicesLoaded,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void RemoteDeviceProvider::OnRemoteDevicesLoaded(
    const RemoteDeviceList& synced_remote_devices) {
  synced_remote_devices_ = synced_remote_devices;
  remote_device_loader_.reset();

  // Notify observers of change. Note that there is no need to check if
  // |synced_remote_devices_| has changed here because the fetch is only started
  // if the change result passed to OnSyncFinished() is CHANGED.
  for (auto& observer : observers_) {
    observer.OnSyncDeviceListChanged();
  }
}

const RemoteDeviceList RemoteDeviceProvider::GetSyncedDevices() const {
  return synced_remote_devices_;
}

}  // namespace cryptauth