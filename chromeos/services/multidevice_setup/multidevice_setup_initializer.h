// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_INITIALIZER_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_INITIALIZER_H_

#include "chromeos/services/device_sync/public/cpp/device_sync_client.h"
#include "chromeos/services/multidevice_setup/multidevice_setup_base.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"

class PrefService;

namespace chromeos {

namespace secure_channel {
class SecureChannelClient;
}  // namespace secure_channel

namespace multidevice_setup {

// Initializes the MultiDeviceSetup service. This class is responsible for
// waiting for asynchronous initialization steps to complete before creating
// the concrete implementation of the mojom::MultiDeviceSetup interface.
class MultiDeviceSetupInitializer
    : public MultiDeviceSetupBase,
      public device_sync::DeviceSyncClient::Observer {
 public:
  class Factory {
   public:
    static Factory* Get();
    static void SetFactoryForTesting(Factory* test_factory);
    virtual ~Factory();
    virtual std::unique_ptr<MultiDeviceSetupBase> BuildInstance(
        PrefService* pref_service,
        device_sync::DeviceSyncClient* device_sync_client,
        secure_channel::SecureChannelClient* secure_channel_client);

   private:
    static Factory* test_factory_;
  };

  ~MultiDeviceSetupInitializer() override;

 private:
  MultiDeviceSetupInitializer(
      PrefService* pref_service,
      device_sync::DeviceSyncClient* device_sync_client,
      secure_channel::SecureChannelClient* secure_channel_client);

  // mojom::MultiDeviceSetup:
  void SetAccountStatusChangeDelegate(
      mojom::AccountStatusChangeDelegatePtr delegate) override;
  void TriggerEventForDebugging(
      mojom::EventTypeForDebugging type,
      TriggerEventForDebuggingCallback callback) override;

  // device_sync::DeviceSyncClient::Observer:
  void OnReady() override;

  void InitializeImplementation();

  PrefService* pref_service_;
  device_sync::DeviceSyncClient* device_sync_client_;
  secure_channel::SecureChannelClient* secure_channel_client_;

  std::unique_ptr<mojom::MultiDeviceSetup> multidevice_setup_impl_;

  // If SetAccountStatusChangeDelegate() is called before initialization is
  // complete, |pending_delegate_| stores the pointer until it can be passed to
  // the full MultiDeviceSetup implementation.
  mojom::AccountStatusChangeDelegatePtr pending_delegate_;

  DISALLOW_COPY_AND_ASSIGN(MultiDeviceSetupInitializer);
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_INITIALIZER_H_
