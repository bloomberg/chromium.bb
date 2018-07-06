// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_INITIALIZER_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_INITIALIZER_H_

#include <vector>

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
  void AddHostStatusObserver(mojom::HostStatusObserverPtr observer) override;
  void GetEligibleHostDevices(GetEligibleHostDevicesCallback callback) override;
  void SetHostDevice(const std::string& host_public_key,
                     SetHostDeviceCallback callback) override;
  void RemoveHostDevice() override;
  void GetHostStatus(GetHostStatusCallback callback) override;
  void RetrySetHostNow(RetrySetHostNowCallback callback) override;
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

  // If API functions are called before initialization is complete, their
  // parameters are cached here. Once asynchronous initialization is complete,
  // the parameters are passed to |multidevice_setup_impl_|.
  mojom::AccountStatusChangeDelegatePtr pending_delegate_;
  std::vector<mojom::HostStatusObserverPtr> pending_observers_;
  std::vector<GetEligibleHostDevicesCallback> pending_get_eligible_hosts_args_;
  std::vector<GetHostStatusCallback> pending_get_host_args_;
  std::vector<RetrySetHostNowCallback> pending_retry_set_host_args_;

  // Special case: for SetHostDevice() and RemoveHostDevice(), only keep track
  // of the most recent call. Since each call to either of these functions
  // overwrites the previous call, only one needs to be passed.
  base::Optional<std::pair<std::string, SetHostDeviceCallback>>
      pending_set_host_args_;
  bool pending_should_remove_host_device_ = false;

  DISALLOW_COPY_AND_ASSIGN(MultiDeviceSetupInitializer);
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_INITIALIZER_H_
