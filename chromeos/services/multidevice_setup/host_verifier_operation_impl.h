// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_HOST_VERIFIER_OPERATION_IMPL_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_HOST_VERIFIER_OPERATION_IMPL_H_

#include <memory>
#include <ostream>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/timer/timer.h"
#include "chromeos/services/multidevice_setup/host_verifier_operation.h"
#include "chromeos/services/multidevice_setup/proto/multidevice_setup.pb.h"
#include "chromeos/services/secure_channel/public/cpp/client/client_channel.h"
#include "chromeos/services/secure_channel/public/cpp/client/connection_attempt.h"
#include "chromeos/services/secure_channel/public/mojom/secure_channel.mojom.h"
#include "components/cryptauth/remote_device_ref.h"

namespace chromeos {

namespace device_sync {
class DeviceSyncClient;
}  // namespace device_sync

namespace secure_channel {
class SecureChannelClient;
}  // namespace secure_channel

namespace multidevice_setup {

// Concrete HostVerifierOperation implementation. To verify the host, this class
// performs the following steps:
// (1) Call FindEligibleDevices(). This step sends a message to the host device,
//     which in turn enables background advertising.
// (2) Creates a connection to the device using the BLE listener role.
// (3) Sends an EnableBetterTogetherRequest message to the host device.
// (4) Waits for an EnableBetterTogetherResponse messages to be returned by the
//     host device.
class HostVerifierOperationImpl
    : public HostVerifierOperation,
      public secure_channel::ConnectionAttempt::Delegate,
      public secure_channel::ClientChannel::Observer {
 public:
  class Factory {
   public:
    static Factory* Get();
    static void SetFactoryForTesting(Factory* test_factory);
    virtual ~Factory();
    virtual std::unique_ptr<HostVerifierOperation> BuildInstance(
        HostVerifierOperation::Delegate* delegate,
        cryptauth::RemoteDeviceRef device_to_connect,
        cryptauth::RemoteDeviceRef local_device,
        device_sync::DeviceSyncClient* device_sync_client,
        secure_channel::SecureChannelClient* secure_channel_client,
        std::unique_ptr<base::OneShotTimer> timer =
            std::make_unique<base::OneShotTimer>());

   private:
    static Factory* test_factory_;
  };

  ~HostVerifierOperationImpl() override;

 private:
  friend class MultiDeviceSetupHostVerifierOperationImplTest;

  enum class Status {
    kWaitingForFindEligibleDevicesResponse,
    kWaitingForConnection,
    kWaitingForResponse,
    kFinished
  };
  friend std::ostream& operator<<(std::ostream& stream, const Status& status);

  HostVerifierOperationImpl(
      HostVerifierOperation::Delegate* delegate,
      cryptauth::RemoteDeviceRef device_to_connect,
      cryptauth::RemoteDeviceRef local_device,
      device_sync::DeviceSyncClient* device_sync_client,
      secure_channel::SecureChannelClient* secure_channel_client,
      std::unique_ptr<base::OneShotTimer> timer);

  static BetterTogetherSetupMessageWrapper
  CreateWrappedEnableBetterTogetherRequest();

  // HostVerifierOperation:
  void PerformCancelOperation() override;

  // secure_channel::ConnectionAttempt::Delegate:
  void OnConnectionAttemptFailure(
      secure_channel::mojom::ConnectionAttemptFailureReason reason) override;
  void OnConnection(
      std::unique_ptr<secure_channel::ClientChannel> channel) override;

  // secure_channel::ClientChannel::Observer:
  void OnDisconnected() override;
  void OnMessageReceived(const std::string& payload) override;

  void OnTimeout();
  void OnFindEligibleDevicesResponse(
      const base::Optional<std::string>& error_code,
      cryptauth::RemoteDeviceRefList eligible_devices,
      cryptauth::RemoteDeviceRefList ineligible_devices);
  void FinishOperation(Status expected_current_status, Result result);
  void TransitionStatus(Status expected_current_status, Status new_status);

  cryptauth::RemoteDeviceRef device_to_connect_;
  cryptauth::RemoteDeviceRef local_device_;
  device_sync::DeviceSyncClient* device_sync_client_;
  secure_channel::SecureChannelClient* secure_channel_client_;
  std::unique_ptr<base::OneShotTimer> timer_;

  Status status_ = Status::kWaitingForFindEligibleDevicesResponse;
  std::unique_ptr<secure_channel::ConnectionAttempt> connection_attempt_;
  std::unique_ptr<secure_channel::ClientChannel> client_channel_;

  base::WeakPtrFactory<HostVerifierOperationImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(HostVerifierOperationImpl);
};

std::ostream& operator<<(std::ostream& stream,
                         const HostVerifierOperationImpl::Status& status);

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_HOST_VERIFIER_OPERATION_IMPL_H_
