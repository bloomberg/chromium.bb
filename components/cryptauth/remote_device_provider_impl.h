// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRYPTAUTH_REMOTE_DEVICE_PROVIDER_IMPL_H_
#define COMPONENTS_CRYPTAUTH_REMOTE_DEVICE_PROVIDER_IMPL_H_

#include "components/cryptauth/cryptauth_device_manager.h"
#include "components/cryptauth/remote_device_provider.h"
#include "components/cryptauth/secure_message_delegate.h"

namespace cryptauth {

class RemoteDeviceLoader;

// Concrete RemoteDeviceProvider implementation.
class RemoteDeviceProviderImpl : public RemoteDeviceProvider,
                                 public CryptAuthDeviceManager::Observer {
 public:
  class Factory {
   public:
    static std::unique_ptr<RemoteDeviceProvider> NewInstance(
        CryptAuthDeviceManager* device_manager,
        const std::string& user_id,
        const std::string& user_private_key,
        SecureMessageDelegate::Factory* secure_message_delegate_factory);

    static void SetInstanceForTesting(Factory* factory);

   protected:
    virtual std::unique_ptr<RemoteDeviceProvider> BuildInstance(
        CryptAuthDeviceManager* device_manager,
        const std::string& user_id,
        const std::string& user_private_key,
        SecureMessageDelegate::Factory* secure_message_delegate_factory);

   private:
    static Factory* factory_instance_;
  };

  RemoteDeviceProviderImpl(
      CryptAuthDeviceManager* device_manager,
      const std::string& user_id,
      const std::string& user_private_key,
      SecureMessageDelegate::Factory* secure_message_delegate_factory);

  ~RemoteDeviceProviderImpl() override;

  // Returns a list of all RemoteDevices that have been synced.
  const RemoteDeviceList& GetSyncedDevices() const override;

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

  SecureMessageDelegate::Factory* secure_message_delegate_factory_;
  std::unique_ptr<RemoteDeviceLoader> remote_device_loader_;
  RemoteDeviceList synced_remote_devices_;
  base::WeakPtrFactory<RemoteDeviceProviderImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RemoteDeviceProviderImpl);
};

}  // namespace cryptauth

#endif  // COMPONENTS_CRYPTAUTH_REMOTE_DEVICE_PROVIDER_IMPL_H_
