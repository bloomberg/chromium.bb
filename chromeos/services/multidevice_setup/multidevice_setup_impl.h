// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_IMPL_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_IMPL_H_

#include <memory>

#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"

class PrefService;

namespace chromeos {

namespace device_sync {
class DeviceSyncClient;
}  // namespace device_sync

namespace secure_channel {
class SecureChannelClient;
}  // namespace secure_channel

namespace multidevice_setup {

class AccountStatusChangeDelegateNotifier;
class HostBackendDelegate;
class SetupFlowCompletionRecorder;

// Concrete MultiDeviceSetup implementation.
class MultiDeviceSetupImpl : public mojom::MultiDeviceSetup {
 public:
  class Factory {
   public:
    static Factory* Get();
    static void SetFactoryForTesting(Factory* test_factory);
    virtual ~Factory();
    virtual std::unique_ptr<mojom::MultiDeviceSetup> BuildInstance(
        PrefService* pref_service,
        device_sync::DeviceSyncClient* device_sync_client,
        secure_channel::SecureChannelClient* secure_channel_client);

   private:
    static Factory* test_factory_;
  };

  ~MultiDeviceSetupImpl() override;

 private:
  MultiDeviceSetupImpl(
      PrefService* pref_service,
      device_sync::DeviceSyncClient* device_sync_client,
      secure_channel::SecureChannelClient* secure_channel_client);

  // mojom::MultiDeviceSetup:
  void SetAccountStatusChangeDelegate(
      mojom::AccountStatusChangeDelegatePtr delegate) override;
  void TriggerEventForDebugging(
      mojom::EventTypeForDebugging type,
      TriggerEventForDebuggingCallback callback) override;

  std::unique_ptr<HostBackendDelegate> host_backend_delegate_;
  std::unique_ptr<SetupFlowCompletionRecorder> setup_flow_completion_recorder_;
  std::unique_ptr<AccountStatusChangeDelegateNotifier> delegate_notifier_;

  DISALLOW_COPY_AND_ASSIGN(MultiDeviceSetupImpl);
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_IMPL_H_
