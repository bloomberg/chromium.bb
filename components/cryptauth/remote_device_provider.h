// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRYPTAUTH_REMOTE_DEVICE_PROVIDER_H_
#define COMPONENTS_CRYPTAUTH_REMOTE_DEVICE_PROVIDER_H_

#include "components/cryptauth/cryptauth_device_manager.h"
#include "components/cryptauth/remote_device.h"

namespace cryptauth {

class RemoteDeviceLoader;
class SecureMessageDelegate;

// TODO(khorimoto): Move SecureMessageDelegateFactory to another file.
class SecureMessageDelegateFactory {
 public:
  virtual std::unique_ptr<SecureMessageDelegate>
  CreateSecureMessageDelegate() = 0;
  virtual ~SecureMessageDelegateFactory(){};
};

// This class generates and caches RemoteDevice objects when associated metadata
// has been synced, and updates this cache when a new sync occurs.
class RemoteDeviceProvider : public CryptAuthDeviceManager::Observer {
 public:
  class Observer {
   public:
    virtual void OnSyncDeviceListChanged() {}

   protected:
    virtual ~Observer() {}
  };

  RemoteDeviceProvider(
      CryptAuthDeviceManager* device_manager,
      const std::string& user_id,
      const std::string& user_private_key,
      SecureMessageDelegateFactory* secure_message_delegate_factory);

  ~RemoteDeviceProvider() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns a list of all RemoteDevices that have been synced.
  const RemoteDeviceList GetSyncedDevices() const;

  // CryptAuthDeviceManager::Observer:
  void OnSyncFinished(
      CryptAuthDeviceManager::SyncResult sync_result,
      CryptAuthDeviceManager::DeviceChangeResult device_change_result) override;

 private:
  void OnRemoteDevicesLoaded(const RemoteDeviceList& synced_remote_devices);

  // To get ExternalDeviceInfo needed to retrieve RemoteDevices.
  CryptAuthDeviceManager* device_manager_;

  // The account ID of the current user.
  const std::string user_id_;

  // The private key used to generate RemoteDevices.
  const std::string user_private_key_;

  SecureMessageDelegateFactory* secure_message_delegate_factory_;
  std::unique_ptr<RemoteDeviceLoader> remote_device_loader_;
  RemoteDeviceList synced_remote_devices_;
  base::ObserverList<Observer> observers_;
  base::WeakPtrFactory<RemoteDeviceProvider> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RemoteDeviceProvider);
};

}  // namespace cryptauth

#endif  // COMPONENTS_CRYPTAUTH_REMOTE_DEVICE_PROVIDER_H_
