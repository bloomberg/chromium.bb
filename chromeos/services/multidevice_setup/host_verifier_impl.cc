// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/host_verifier_impl.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "chromeos/services/device_sync/public/cpp/device_sync_client.h"
#include "chromeos/services/secure_channel/public/cpp/client/secure_channel_client.h"

namespace chromeos {

namespace multidevice_setup {

// static
HostVerifierImpl::Factory* HostVerifierImpl::Factory::test_factory_ = nullptr;

// static
HostVerifierImpl::Factory* HostVerifierImpl::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<Factory> factory;
  return factory.get();
}

// static
void HostVerifierImpl::Factory::SetFactoryForTesting(Factory* test_factory) {
  test_factory_ = test_factory;
}

HostVerifierImpl::Factory::~Factory() = default;

std::unique_ptr<HostVerifier> HostVerifierImpl::Factory::BuildInstance(
    HostBackendDelegate* host_backend_delegate,
    device_sync::DeviceSyncClient* device_sync_client,
    secure_channel::SecureChannelClient* secure_channel_client) {
  return base::WrapUnique(new HostVerifierImpl(
      host_backend_delegate, device_sync_client, secure_channel_client));
}

HostVerifierImpl::HostVerifierImpl(
    HostBackendDelegate* host_backend_delegate,
    device_sync::DeviceSyncClient* device_sync_client,
    secure_channel::SecureChannelClient* secure_channel_client)
    : host_backend_delegate_(host_backend_delegate),
      device_sync_client_(device_sync_client),
      secure_channel_client_(secure_channel_client) {
  host_backend_delegate_->AddObserver(this);
}

HostVerifierImpl::~HostVerifierImpl() {
  host_backend_delegate_->RemoveObserver(this);
}

bool HostVerifierImpl::IsHostVerified() {
  NOTIMPLEMENTED();

  // Use both |device_sync_client_| and |secure_channel_client_| to prevent
  // unused field compiler warning.
  return static_cast<void*>(device_sync_client_) ==
         static_cast<void*>(secure_channel_client_);
}

void HostVerifierImpl::PerformAttemptVerificationNow() {
  NOTIMPLEMENTED();
}

void HostVerifierImpl::OnHostChangedOnBackend() {
  NOTIMPLEMENTED();
}

}  // namespace multidevice_setup

}  // namespace chromeos
