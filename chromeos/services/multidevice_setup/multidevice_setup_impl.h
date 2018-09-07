// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_IMPL_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_IMPL_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "chromeos/services/multidevice_setup/feature_state_manager.h"
#include "chromeos/services/multidevice_setup/host_status_provider.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "url/gurl.h"

class PrefService;

namespace cryptauth {
class GcmDeviceInfoProvider;
}  // namespace cryptauth

namespace chromeos {

namespace device_sync {
class DeviceSyncClient;
}  // namespace device_sync

namespace secure_channel {
class SecureChannelClient;
}  // namespace secure_channel

namespace multidevice_setup {

class AccountStatusChangeDelegateNotifier;
class AndroidSmsAppHelperDelegate;
class AuthTokenValidator;
class DeviceReenroller;
class HostBackendDelegate;
class HostStatusProvider;
class HostVerifier;
class EligibleHostDevicesProvider;
class SetupFlowCompletionRecorder;

// Concrete MultiDeviceSetup implementation.
class MultiDeviceSetupImpl : public mojom::MultiDeviceSetup,
                             public HostStatusProvider::Observer,
                             public FeatureStateManager::Observer {
 public:
  class Factory {
   public:
    static Factory* Get();
    static void SetFactoryForTesting(Factory* test_factory);
    virtual ~Factory();
    virtual std::unique_ptr<mojom::MultiDeviceSetup> BuildInstance(
        PrefService* pref_service,
        device_sync::DeviceSyncClient* device_sync_client,
        secure_channel::SecureChannelClient* secure_channel_client,
        AuthTokenValidator* auth_token_validator,
        std::unique_ptr<AndroidSmsAppHelperDelegate>
            android_sms_app_helper_delegate,
        const cryptauth::GcmDeviceInfoProvider* gcm_device_info_provider);

   private:
    static Factory* test_factory_;
  };

  ~MultiDeviceSetupImpl() override;

 private:
  friend class MultiDeviceSetupImplTest;

  // TODO(crbug.com/874283): Remove SecureChannelClient injection.
  MultiDeviceSetupImpl(
      PrefService* pref_service,
      device_sync::DeviceSyncClient* device_sync_client,
      secure_channel::SecureChannelClient* secure_channel_client,
      AuthTokenValidator* auth_token_validator,
      std::unique_ptr<AndroidSmsAppHelperDelegate>
          android_sms_app_helper_delegate,
      const cryptauth::GcmDeviceInfoProvider* gcm_device_info_provider);

  // mojom::MultiDeviceSetup:
  void SetAccountStatusChangeDelegate(
      mojom::AccountStatusChangeDelegatePtr delegate) override;
  void AddHostStatusObserver(mojom::HostStatusObserverPtr observer) override;
  void AddFeatureStateObserver(
      mojom::FeatureStateObserverPtr observer) override;
  void GetEligibleHostDevices(GetEligibleHostDevicesCallback callback) override;
  void SetHostDevice(const std::string& host_device_id,
                     const std::string& auth_token,
                     SetHostDeviceCallback callback) override;
  void RemoveHostDevice() override;
  void GetHostStatus(GetHostStatusCallback callback) override;
  void SetFeatureEnabledState(mojom::Feature feature,
                              bool enabled,
                              const base::Optional<std::string>& auth_token,
                              SetFeatureEnabledStateCallback callback) override;
  void GetFeatureStates(GetFeatureStatesCallback callback) override;
  void RetrySetHostNow(RetrySetHostNowCallback callback) override;
  void TriggerEventForDebugging(
      mojom::EventTypeForDebugging type,
      TriggerEventForDebuggingCallback callback) override;

  // HostStatusProvider::Observer:
  void OnHostStatusChange(const HostStatusProvider::HostStatusWithDevice&
                              host_status_with_device) override;

  // FeatureStateManager::Observer:
  void OnFeatureStatesChange(
      const FeatureStateManager::FeatureStatesMap& feature_states_map) override;

  void FlushForTesting();

  std::unique_ptr<AndroidSmsAppHelperDelegate> android_sms_app_helper_delegate_;
  std::unique_ptr<EligibleHostDevicesProvider> eligible_host_devices_provider_;
  std::unique_ptr<HostBackendDelegate> host_backend_delegate_;
  std::unique_ptr<HostVerifier> host_verifier_;
  std::unique_ptr<HostStatusProvider> host_status_provider_;
  std::unique_ptr<FeatureStateManager> feature_state_manager_;
  std::unique_ptr<SetupFlowCompletionRecorder> setup_flow_completion_recorder_;
  std::unique_ptr<AccountStatusChangeDelegateNotifier> delegate_notifier_;
  std::unique_ptr<DeviceReenroller> device_reenroller_;
  AuthTokenValidator* auth_token_validator_;

  mojo::InterfacePtrSet<mojom::HostStatusObserver> host_status_observers_;
  mojo::InterfacePtrSet<mojom::FeatureStateObserver> feature_state_observers_;

  DISALLOW_COPY_AND_ASSIGN(MultiDeviceSetupImpl);
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_IMPL_H_
