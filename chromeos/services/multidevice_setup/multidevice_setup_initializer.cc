// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/multidevice_setup_initializer.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "chromeos/components/proximity_auth/logging/logging.h"
#include "chromeos/services/multidevice_setup/multidevice_setup_impl.h"

namespace chromeos {

namespace multidevice_setup {

// static
MultiDeviceSetupInitializer::Factory*
    MultiDeviceSetupInitializer::Factory::test_factory_ = nullptr;

// static
MultiDeviceSetupInitializer::Factory*
MultiDeviceSetupInitializer::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<Factory> factory;
  return factory.get();
}

// static
void MultiDeviceSetupInitializer::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

MultiDeviceSetupInitializer::Factory::~Factory() = default;

std::unique_ptr<MultiDeviceSetupBase>
MultiDeviceSetupInitializer::Factory::BuildInstance(
    PrefService* pref_service,
    device_sync::DeviceSyncClient* device_sync_client,
    secure_channel::SecureChannelClient* secure_channel_client) {
  return base::WrapUnique(new MultiDeviceSetupInitializer(
      pref_service, device_sync_client, secure_channel_client));
}

MultiDeviceSetupInitializer::MultiDeviceSetupInitializer(
    PrefService* pref_service,
    device_sync::DeviceSyncClient* device_sync_client,
    secure_channel::SecureChannelClient* secure_channel_client)
    : pref_service_(pref_service),
      device_sync_client_(device_sync_client),
      secure_channel_client_(secure_channel_client) {
  if (device_sync_client_->is_ready()) {
    InitializeImplementation();
    return;
  }

  device_sync_client_->AddObserver(this);
}

MultiDeviceSetupInitializer::~MultiDeviceSetupInitializer() = default;

void MultiDeviceSetupInitializer::SetAccountStatusChangeDelegate(
    mojom::AccountStatusChangeDelegatePtr delegate) {
  if (multidevice_setup_impl_) {
    multidevice_setup_impl_->SetAccountStatusChangeDelegate(
        std::move(delegate));
    return;
  }

  PA_LOG(INFO) << "MultiDeviceSetupInitializer::"
               << "SetAccountStatusChangeDelegate(): Delegate set before "
               << "service was initialized; will be set once initialization is "
               << "complete.";
  pending_delegate_ = std::move(delegate);
}

void MultiDeviceSetupInitializer::TriggerEventForDebugging(
    mojom::EventTypeForDebugging type,
    TriggerEventForDebuggingCallback callback) {
  if (multidevice_setup_impl_) {
    multidevice_setup_impl_->TriggerEventForDebugging(type,
                                                      std::move(callback));
    return;
  }

  // If initialization is not complete, no debug event can be sent.
  std::move(callback).Run(false /* success */);
}

void MultiDeviceSetupInitializer::OnReady() {
  device_sync_client_->RemoveObserver(this);
  InitializeImplementation();
}

void MultiDeviceSetupInitializer::InitializeImplementation() {
  DCHECK(!multidevice_setup_impl_);

  multidevice_setup_impl_ = MultiDeviceSetupImpl::Factory::Get()->BuildInstance(
      pref_service_, device_sync_client_, secure_channel_client_);

  if (!pending_delegate_)
    return;

  multidevice_setup_impl_->SetAccountStatusChangeDelegate(
      std::move(pending_delegate_));
}

}  // namespace multidevice_setup

}  // namespace chromeos
