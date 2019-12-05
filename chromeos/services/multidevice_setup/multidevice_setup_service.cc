// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/multidevice_setup_service.h"

#include "base/bind.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/services/multidevice_setup/account_status_change_delegate_notifier_impl.h"
#include "chromeos/services/multidevice_setup/device_reenroller.h"
#include "chromeos/services/multidevice_setup/grandfathered_easy_unlock_host_disabler.h"
#include "chromeos/services/multidevice_setup/host_backend_delegate_impl.h"
#include "chromeos/services/multidevice_setup/host_device_timestamp_manager_impl.h"
#include "chromeos/services/multidevice_setup/host_verifier_impl.h"
#include "chromeos/services/multidevice_setup/multidevice_setup_base.h"
#include "chromeos/services/multidevice_setup/multidevice_setup_initializer.h"
#include "chromeos/services/multidevice_setup/privileged_host_device_setter_impl.h"
#include "chromeos/services/multidevice_setup/public/cpp/android_sms_app_helper_delegate.h"
#include "chromeos/services/multidevice_setup/public/cpp/android_sms_pairing_state_tracker.h"
#include "chromeos/services/multidevice_setup/public/cpp/prefs.h"

namespace chromeos {

namespace multidevice_setup {

// static
void MultiDeviceSetupService::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  HostDeviceTimestampManagerImpl::RegisterPrefs(registry);
  AccountStatusChangeDelegateNotifierImpl::RegisterPrefs(registry);
  HostBackendDelegateImpl::RegisterPrefs(registry);
  HostVerifierImpl::RegisterPrefs(registry);
  GrandfatheredEasyUnlockHostDisabler::RegisterPrefs(registry);
  RegisterFeaturePrefs(registry);
}

MultiDeviceSetupService::MultiDeviceSetupService(
    PrefService* pref_service,
    device_sync::DeviceSyncClient* device_sync_client,
    AuthTokenValidator* auth_token_validator,
    OobeCompletionTracker* oobe_completion_tracker,
    AndroidSmsAppHelperDelegate* android_sms_app_helper_delegate,
    AndroidSmsPairingStateTracker* android_sms_pairing_state_tracker,
    const device_sync::GcmDeviceInfoProvider* gcm_device_info_provider)
    : multidevice_setup_(
          MultiDeviceSetupInitializer::Factory::Get()->BuildInstance(
              pref_service,
              device_sync_client,
              auth_token_validator,
              oobe_completion_tracker,
              android_sms_app_helper_delegate,
              android_sms_pairing_state_tracker,
              gcm_device_info_provider)),
      privileged_host_device_setter_(
          PrivilegedHostDeviceSetterImpl::Factory::Get()->BuildInstance(
              multidevice_setup_.get())) {}

MultiDeviceSetupService::~MultiDeviceSetupService() {
  // Subclasses may hold onto message response callbacks. It's important that
  // all receivers are closed by the time those callbacks are destroyed, or they
  // will DCHECK.
  if (multidevice_setup_)
    multidevice_setup_->CloseAllReceivers();
}

void MultiDeviceSetupService::BindMultiDeviceSetup(
    mojo::PendingReceiver<mojom::MultiDeviceSetup> receiver) {
  multidevice_setup_->BindReceiver(std::move(receiver));
}

void MultiDeviceSetupService::BindPrivilegedHostDeviceSetter(
    mojo::PendingReceiver<mojom::PrivilegedHostDeviceSetter> receiver) {
  privileged_host_device_setter_->BindReceiver(std::move(receiver));
}

}  // namespace multidevice_setup

}  // namespace chromeos
