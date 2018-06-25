// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_HOST_VERIFIER_IMPL_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_HOST_VERIFIER_IMPL_H_

#include "base/macros.h"
#include "chromeos/services/multidevice_setup/host_backend_delegate.h"
#include "chromeos/services/multidevice_setup/host_verifier.h"

namespace chromeos {

namespace device_sync {
class DeviceSyncClient;
}  // namespace device_sync

namespace secure_channel {
class SecureChannelClient;
}  // namespace secure_channel

namespace multidevice_setup {

// Concrete HostVerifier implementation, which starts trying to verify a host as
// soon as it is set on the back-end. If verification fails, HostVerifierImpl
// uses an exponential back-off to retry verification until it succeeds.
//
// If the MultiDevice host is changed while verification is in progress, the
// previous verification attempt is canceled and a new attempt begins with the
// updated device.
//
// TODO(khorimoto): Fill out implementation.
class HostVerifierImpl : public HostVerifier,
                         public HostBackendDelegate::Observer {
 public:
  class Factory {
   public:
    static Factory* Get();
    static void SetFactoryForTesting(Factory* test_factory);
    virtual ~Factory();
    virtual std::unique_ptr<HostVerifier> BuildInstance(
        HostBackendDelegate* host_backend_delegate,
        device_sync::DeviceSyncClient* device_sync_client,
        secure_channel::SecureChannelClient* secure_channel_client);

   private:
    static Factory* test_factory_;
  };

  ~HostVerifierImpl() override;

 private:
  HostVerifierImpl(HostBackendDelegate* host_backend_delegate,
                   device_sync::DeviceSyncClient* device_sync_client,
                   secure_channel::SecureChannelClient* secure_channel_client);

  // HostVerifier:
  bool IsHostVerified() override;
  void PerformAttemptVerificationNow() override;

  // HostBackendDelegate::Observer:
  void OnHostChangedOnBackend() override;

  HostBackendDelegate* host_backend_delegate_;
  device_sync::DeviceSyncClient* device_sync_client_;
  secure_channel::SecureChannelClient* secure_channel_client_;

  DISALLOW_COPY_AND_ASSIGN(HostVerifierImpl);
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_HOST_VERIFIER_IMPL_H_
