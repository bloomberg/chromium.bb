// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/host_verifier_operation_impl.h"

#include <sstream>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "chromeos/components/proximity_auth/logging/logging.h"
#include "chromeos/services/device_sync/public/cpp/device_sync_client.h"
#include "chromeos/services/secure_channel/public/cpp/client/secure_channel_client.h"

namespace chromeos {

namespace multidevice_setup {

namespace {

const char kFeature[] = "better_together_setup";

const int kNumMinutesForTimeout = 1;

const char kEligibleDeviceIdsLogString[] = "Eligible device IDs";
const char kIneligibleDeviceIdsLogString[] = "Ineligible device IDs";

void LogDeviceIds(const cryptauth::RemoteDeviceRefList& device_list,
                  const std::string& device_type_name,
                  std::stringstream* ss) {
  *ss << device_type_name << ": [";
  if (!device_list.empty()) {
    for (const auto& device : device_list)
      *ss << "\"" << device.GetTruncatedDeviceIdForLogs() << "\", ";
    ss->seekp(-2, ss->cur);  // Remove last ", " from the stream.
  }
  *ss << "]";
}

std::string CreateLogString(
    const cryptauth::RemoteDeviceRefList& eligible_devices,
    const cryptauth::RemoteDeviceRefList& ineligible_devices) {
  std::stringstream ss;
  LogDeviceIds(eligible_devices, kEligibleDeviceIdsLogString, &ss);
  ss << ", ";
  LogDeviceIds(ineligible_devices, kIneligibleDeviceIdsLogString, &ss);
  return ss.str();
}

base::Optional<EnableBetterTogetherResponse> DeserializePossibleResponse(
    const std::string& payload) {
  BetterTogetherSetupMessageWrapper wrapper;

  // If |payload| does not correspond to a BetterTogetherSetupMessageWrapper,
  // return null.
  if (!wrapper.ParseFromString(payload))
    return base::nullopt;

  // If |wrapper|'s type indicates that it does not contain a
  // EnableBetterTogetherResponse, return null.
  if (!wrapper.has_type() ||
      wrapper.type() != MessageType::ENABLE_BETTER_TOGETHER_RESPONSE) {
    return base::nullopt;
  }

  EnableBetterTogetherResponse response;

  // If |wrapper|'s payload does not represent an EnableBetterTogetherResponse,
  // return null.
  if (!wrapper.has_payload() || !response.ParseFromString(wrapper.payload()))
    return base::nullopt;

  return response;
}

}  // namespace

// static
HostVerifierOperationImpl::Factory*
    HostVerifierOperationImpl::Factory::test_factory_ = nullptr;

// static
HostVerifierOperationImpl::Factory* HostVerifierOperationImpl::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<Factory> factory;
  return factory.get();
}

// static
void HostVerifierOperationImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

HostVerifierOperationImpl::Factory::~Factory() = default;

std::unique_ptr<HostVerifierOperation>
HostVerifierOperationImpl::Factory::BuildInstance(
    HostVerifierOperation::Delegate* delegate,
    cryptauth::RemoteDeviceRef device_to_connect,
    cryptauth::RemoteDeviceRef local_device,
    device_sync::DeviceSyncClient* device_sync_client,
    secure_channel::SecureChannelClient* secure_channel_client,
    std::unique_ptr<base::OneShotTimer> timer) {
  return base::WrapUnique(new HostVerifierOperationImpl(
      delegate, device_to_connect, local_device, device_sync_client,
      secure_channel_client, std::move(timer)));
}

// static
BetterTogetherSetupMessageWrapper
HostVerifierOperationImpl::CreateWrappedEnableBetterTogetherRequest() {
  BetterTogetherSetupMessageWrapper wrapper;
  wrapper.set_type(MessageType::ENABLE_BETTER_TOGETHER_REQUEST);

  EnableBetterTogetherRequest request;
  wrapper.set_payload(request.SerializeAsString());

  return wrapper;
}

HostVerifierOperationImpl::HostVerifierOperationImpl(
    HostVerifierOperation::Delegate* delegate,
    cryptauth::RemoteDeviceRef device_to_connect,
    cryptauth::RemoteDeviceRef local_device,
    device_sync::DeviceSyncClient* device_sync_client,
    secure_channel::SecureChannelClient* secure_channel_client,
    std::unique_ptr<base::OneShotTimer> timer)
    : HostVerifierOperation(delegate),
      device_to_connect_(device_to_connect),
      local_device_(local_device),
      device_sync_client_(device_sync_client),
      secure_channel_client_(secure_channel_client),
      timer_(std::move(timer)),
      weak_ptr_factory_(this) {
  timer_->Start(FROM_HERE, base::TimeDelta::FromMinutes(kNumMinutesForTimeout),
                base::Bind(&HostVerifierOperationImpl::OnTimeout,
                           base::Unretained(this)));
  device_sync_client_->FindEligibleDevices(
      cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST,
      base::BindOnce(&HostVerifierOperationImpl::OnFindEligibleDevicesResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

HostVerifierOperationImpl::~HostVerifierOperationImpl() = default;

void HostVerifierOperationImpl::PerformCancelOperation() {
  FinishOperation(status_, Result::kCanceled);
}

void HostVerifierOperationImpl::OnConnectionAttemptFailure(
    secure_channel::mojom::ConnectionAttemptFailureReason reason) {
  PA_LOG(WARNING) << "HostVerifierOperationImpl::OnConnectionAttemptFailure(): "
                  << "Failed to establish connection to device with ID \""
                  << device_to_connect_.GetTruncatedDeviceIdForLogs() << "\". "
                  << "Reason: " << reason;
  FinishOperation(Status::kWaitingForConnection,
                  Result::kConnectionAttemptFailed);
}

void HostVerifierOperationImpl::OnConnection(
    std::unique_ptr<secure_channel::ClientChannel> channel) {
  client_channel_ = std::move(channel);
  client_channel_->AddObserver(this);

  TransitionStatus(Status::kWaitingForConnection, Status::kWaitingForResponse);
  client_channel_->SendMessage(
      CreateWrappedEnableBetterTogetherRequest().SerializeAsString(),
      base::DoNothing() /* on_sent_callback */);
}

void HostVerifierOperationImpl::OnDisconnected() {
  // Disconnections may occur after the operation is finished.
  if (status_ == Status::kFinished)
    return;

  PA_LOG(WARNING) << "HostVerifierOperationImpl::OnDisconnected(): "
                  << "Channel disconnected unexpectedly; could not complete "
                  << "verification of device with ID \""
                  << device_to_connect_.GetTruncatedDeviceIdForLogs() << "\". ";
  FinishOperation(Status::kWaitingForResponse,
                  Result::kConnectionDisconnectedUnexpectedly);
}

void HostVerifierOperationImpl::OnMessageReceived(const std::string& payload) {
  base::Optional<EnableBetterTogetherResponse> possible_response =
      DeserializePossibleResponse(payload);

  // The message could have been unrelated; continue waiting.
  if (!possible_response)
    return;

  // If the received message is malformed, fail the operation.
  if (!possible_response->has_result_code() ||
      !EnableBetterTogetherResponse::ResultCode_IsValid(
          possible_response->result_code())) {
    FinishOperation(Status::kWaitingForResponse,
                    Result::kReceivedInvalidResponse);
    return;
  }

  // If the received message includes an error, fail the operation.
  if (possible_response->result_code() == EnableBetterTogetherResponse::ERROR) {
    FinishOperation(Status::kWaitingForResponse,
                    Result::kReceivedErrorResponse);
    return;
  }

  FinishOperation(Status::kWaitingForResponse, Result::kSuccess);
}

void HostVerifierOperationImpl::OnTimeout() {
  switch (status_) {
    case Status::kWaitingForFindEligibleDevicesResponse:
      FinishOperation(status_, Result::kTimeoutFindingEligibleDevices);
      break;
    case Status::kWaitingForConnection:
      FinishOperation(status_, Result::kTimeoutFindingConnection);
      break;
    case Status::kWaitingForResponse:
      FinishOperation(status_, Result::kTimeoutReceivingResponse);
      break;
    case Status::kFinished:
      PA_LOG(ERROR) << "HostVerifierOperationImpl::OnTimeout(): Timeout "
                    << "occurred, but the operation had already finished.";
      NOTREACHED();
      break;
  }
}

void HostVerifierOperationImpl::OnFindEligibleDevicesResponse(
    const base::Optional<std::string>& error_code,
    cryptauth::RemoteDeviceRefList eligible_devices,
    cryptauth::RemoteDeviceRefList ineligible_devices) {
  // A response may be received after the operation is finished.
  if (status_ == Status::kFinished)
    return;

  if (error_code) {
    PA_LOG(WARNING) << "HostVerifierOperationImpl::"
                    << "OnFindEligibleDevicesResponse(): Failed to complete "
                    << "FindEligibleDevices() call. Error code: "
                    << *error_code;
    FinishOperation(Status::kWaitingForFindEligibleDevicesResponse,
                    Result::kErrorCallingFindEligibleDevices);
    return;
  }

  PA_LOG(INFO) << "HostVerifierOperationImpl::OnFindEligibleDevicesResponse(): "
               << "Received FindEligibleDevices() response. "
               << CreateLogString(eligible_devices, ineligible_devices);

  if (!base::ContainsValue(eligible_devices, device_to_connect_)) {
    PA_LOG(WARNING) << "HostVerifierOperationImpl::"
                    << "OnFindEligibleDevicesResponse(): FindEligibleDevices() "
                    << "response does not include the device to connect. ID: "
                    << device_to_connect_.GetTruncatedDeviceIdForLogs();
    FinishOperation(Status::kWaitingForFindEligibleDevicesResponse,
                    Result::kDeviceToVerifyIsNotEligible);
    return;
  }

  TransitionStatus(Status::kWaitingForFindEligibleDevicesResponse,
                   Status::kWaitingForConnection);

  connection_attempt_ = secure_channel_client_->ListenForConnectionFromDevice(
      device_to_connect_, local_device_, kFeature,
      secure_channel::ConnectionPriority::kHigh);
  connection_attempt_->SetDelegate(this);
}

void HostVerifierOperationImpl::FinishOperation(Status expected_current_status,
                                                Result result) {
  TransitionStatus(expected_current_status, Status::kFinished);

  if (client_channel_) {
    client_channel_->RemoveObserver(this);
    client_channel_.reset();
  }

  connection_attempt_.reset();
  timer_->Stop();

  if (result == Result::kCanceled)
    return;

  NotifyOperationFinished(result);
}

void HostVerifierOperationImpl::TransitionStatus(Status expected_current_status,
                                                 Status new_status) {
  if (status_ != expected_current_status) {
    PA_LOG(ERROR) << "HostVerifierOperationImpl::VerifyCurrentStatus(): "
                  << "Current status is unexpected. Current: " << status_
                  << ", Expected: " << expected_current_status
                  << ", Attempted new status: " << new_status;
    NOTREACHED();
  }

  PA_LOG(INFO) << "HostVerifierOperationImpl::TransitionStatus(): "
               << "Transitioning from " << status_ << " to " << new_status
               << ".";
  status_ = new_status;
}

std::ostream& operator<<(std::ostream& stream,
                         const HostVerifierOperationImpl::Status& status) {
  switch (status) {
    case HostVerifierOperationImpl::Status::
        kWaitingForFindEligibleDevicesResponse:
      stream << "[waiting for FindEligibleDevices() response]";
      break;
    case HostVerifierOperationImpl::Status::kWaitingForConnection:
      stream << "[waiting for connection]";
      break;
    case HostVerifierOperationImpl::Status::kWaitingForResponse:
      stream << "[waiting for response]";
      break;
    case HostVerifierOperationImpl::Status::kFinished:
      stream << "[finished]";
      break;
  }
  return stream;
}

}  // namespace multidevice_setup

}  // namespace chromeos
