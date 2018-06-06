// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/secure_channel_impl.h"

#include <ostream>
#include <sstream>

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "chromeos/components/proximity_auth/logging/logging.h"
#include "chromeos/services/secure_channel/active_connection_manager_impl.h"
#include "chromeos/services/secure_channel/client_connection_parameters_impl.h"
#include "chromeos/services/secure_channel/pending_connection_manager_impl.h"
#include "chromeos/services/secure_channel/public/cpp/shared/authenticated_channel.h"

namespace chromeos {

namespace secure_channel {

// static
SecureChannelImpl::Factory* SecureChannelImpl::Factory::test_factory_ = nullptr;

// static
SecureChannelImpl::Factory* SecureChannelImpl::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<Factory> factory;
  return factory.get();
}

// static
void SecureChannelImpl::Factory::SetFactoryForTesting(Factory* test_factory) {
  test_factory_ = test_factory;
}

SecureChannelImpl::Factory::~Factory() = default;

std::unique_ptr<SecureChannelBase> SecureChannelImpl::Factory::BuildInstance() {
  return base::WrapUnique(new SecureChannelImpl());
}

SecureChannelImpl::ConnectionRequestWaitingForDisconnection::
    ConnectionRequestWaitingForDisconnection(
        std::unique_ptr<ClientConnectionParameters>
            client_connection_parameters,
        const std::string& local_device_id,
        ConnectionRole connection_role,
        ConnectionPriority connection_priority)
    : client_connection_parameters(std::move(client_connection_parameters)),
      local_device_id(local_device_id),
      connection_role(connection_role),
      connection_priority(connection_priority) {}

SecureChannelImpl::ConnectionRequestWaitingForDisconnection::
    ConnectionRequestWaitingForDisconnection(
        const ConnectionRequestWaitingForDisconnection& other) {
  // Copy constructor should never be used. Only needed so that compiler will
  // allow adding a std::vector<ConnectionRequestWaitingForDisconnection as a
  // value for a map.
  NOTREACHED();
}

SecureChannelImpl::ConnectionRequestWaitingForDisconnection::
    ~ConnectionRequestWaitingForDisconnection() = default;

SecureChannelImpl::SecureChannelImpl()
    : active_connection_manager_(
          ActiveConnectionManagerImpl::Factory::Get()->BuildInstance(
              this /* delegate */)),
      pending_connection_manager_(
          PendingConnectionManagerImpl::Factory::Get()->BuildInstance(
              this /* delegate */)),
      remote_device_cache_(
          cryptauth::RemoteDeviceCache::Factory::Get()->BuildInstance()) {}

SecureChannelImpl::~SecureChannelImpl() = default;

void SecureChannelImpl::ListenForConnectionFromDevice(
    const cryptauth::RemoteDevice& device_to_connect,
    const cryptauth::RemoteDevice& local_device,
    const std::string& feature,
    ConnectionPriority connection_priority,
    mojom::ConnectionDelegatePtr delegate) {
  ProcessConnectionRequest(
      ApiFunctionName::kListenForConnection, device_to_connect, local_device,
      ClientConnectionParametersImpl::Factory::Get()->BuildInstance(
          feature, std::move(delegate)),
      ConnectionRole::kListenerRole, connection_priority);
}

void SecureChannelImpl::InitiateConnectionToDevice(
    const cryptauth::RemoteDevice& device_to_connect,
    const cryptauth::RemoteDevice& local_device,
    const std::string& feature,
    ConnectionPriority connection_priority,
    mojom::ConnectionDelegatePtr delegate) {
  ProcessConnectionRequest(
      ApiFunctionName::kInitiateConnection, device_to_connect, local_device,
      ClientConnectionParametersImpl::Factory::Get()->BuildInstance(
          feature, std::move(delegate)),
      ConnectionRole::kInitiatorRole, connection_priority);
}

void SecureChannelImpl::OnDisconnected(
    const ConnectionDetails& connection_details) {
  auto pending_requests_it =
      disconnecting_details_to_requests_map_.find(connection_details);
  if (pending_requests_it == disconnecting_details_to_requests_map_.end()) {
    // If there were no queued client requests that were waiting for a
    // disconnection, there is nothing to do.
    PA_LOG(INFO) << "SecureChannelImpl::OnDisconnected(): Previously-active "
                 << "connection became disconnected. Details: "
                 << connection_details;
    return;
  }

  // For each request which was pending (i.e., waiting for a disconnecting
  // connection to disconnect), pass the request off to
  // PendingConnectionManager.
  for (auto& details : pending_requests_it->second) {
    PA_LOG(INFO) << "SecureChannelImpl::OnDisconnected(): Disconnection "
                 << "completed; starting pending connection attempt. Request: "
                 << *details.client_connection_parameters
                 << ", Local device ID: \""
                 << cryptauth::RemoteDeviceRef::TruncateDeviceIdForLogs(
                        details.local_device_id)
                 << "\""
                 << ", Role: " << details.connection_role
                 << ", Priority: " << details.connection_priority
                 << ", Details: " << connection_details;
    pending_connection_manager_->HandleConnectionRequest(
        connection_details, details.local_device_id,
        std::move(details.client_connection_parameters),
        details.connection_role, details.connection_priority);
  }

  // Now that each item has been passed on, remove the map entry.
  disconnecting_details_to_requests_map_.erase(connection_details);
}

void SecureChannelImpl::OnConnection(
    std::unique_ptr<AuthenticatedChannel> authenticated_channel,
    std::vector<std::unique_ptr<ClientConnectionParameters>> clients,
    const ConnectionDetails& connection_details) {
  ActiveConnectionManager::ConnectionState state =
      active_connection_manager_->GetConnectionState(connection_details);
  if (state != ActiveConnectionManager::ConnectionState::kNoConnectionExists) {
    PA_LOG(ERROR) << "SecureChannelImpl::OnConnection(): Connection created "
                  << "for detail " << connection_details << ", but a "
                  << "connection already existed for those details.";
    NOTREACHED();
  }

  // Build string of clients whose connection attempts succeeded.
  std::stringstream ss;
  ss << "[";
  for (const auto& client : clients)
    ss << *client << ", ";
  ss.seekp(-2, ss.cur);  // Remove last ", " from the stream.
  ss << "]";

  PA_LOG(INFO) << "SecureChannelImpl::OnConnection(): Connection created "
               << "successfully. Details: " << connection_details
               << ", Clients: " << ss.str();
  active_connection_manager_->AddActiveConnection(
      std::move(authenticated_channel), std::move(clients), connection_details);
}

void SecureChannelImpl::ProcessConnectionRequest(
    ApiFunctionName api_fn_name,
    const cryptauth::RemoteDevice& device_to_connect,
    const cryptauth::RemoteDevice& local_device,
    std::unique_ptr<ClientConnectionParameters> client_connection_parameters,
    ConnectionRole connection_role,
    ConnectionPriority connection_priority) {
  // Check 1: Is the provided ConnectionDelegate valid? If not, return early.
  if (CheckForInvalidRequest(api_fn_name, client_connection_parameters.get()))
    return;

  // Check 2: Is the provided device to connect valid? If not, notify client and
  // return early.
  if (CheckForInvalidInputDevice(api_fn_name, device_to_connect,
                                 client_connection_parameters.get(),
                                 false /* is_local_device */)) {
    return;
  }

  // Check 3: Is the provided local device valid? If not, notify client and
  // return early.
  if (CheckForInvalidInputDevice(api_fn_name, local_device,
                                 client_connection_parameters.get(),
                                 true /* is_local_device */)) {
    return;
  }

  // At this point, the request has been deemed valid.
  ConnectionDetails connection_details(device_to_connect.GetDeviceId(),
                                       ConnectionMedium::kBluetoothLowEnergy);
  switch (active_connection_manager_->GetConnectionState(connection_details)) {
    case ActiveConnectionManager::ConnectionState::kActiveConnectionExists:
      PA_LOG(INFO) << "SecureChannelImpl::" << api_fn_name << "(): Adding "
                   << "request to active channel. Request: "
                   << *client_connection_parameters << ", Local device ID: \""
                   << cryptauth::RemoteDeviceRef::TruncateDeviceIdForLogs(
                          local_device.GetDeviceId())
                   << "\""
                   << ", Role: " << connection_role
                   << ", Priority: " << connection_priority
                   << ", Details: " << connection_details;
      active_connection_manager_->AddClientToChannel(
          std::move(client_connection_parameters), connection_details);
      break;

    case ActiveConnectionManager::ConnectionState::kNoConnectionExists:
      PA_LOG(INFO) << "SecureChannelImpl::" << api_fn_name << "(): Starting "
                   << "pending connection attempt. Request: "
                   << *client_connection_parameters << ", Local device ID: \""
                   << cryptauth::RemoteDeviceRef::TruncateDeviceIdForLogs(
                          local_device.GetDeviceId())
                   << "\""
                   << ", Role: " << connection_role
                   << ", Priority: " << connection_priority
                   << ", Details: " << connection_details;
      pending_connection_manager_->HandleConnectionRequest(
          connection_details, local_device.GetDeviceId(),
          std::move(client_connection_parameters), connection_role,
          connection_priority);
      break;

    case ActiveConnectionManager::ConnectionState::
        kDisconnectingConnectionExists:
      PA_LOG(INFO) << "SecureChannelImpl::" << api_fn_name << "(): Received "
                   << "request for which a disconnecting connection exists. "
                   << "Waiting for connection to disconnect completely before "
                   << "continuing. Request: " << *client_connection_parameters
                   << ", Local device ID: \""
                   << cryptauth::RemoteDeviceRef::TruncateDeviceIdForLogs(
                          local_device.GetDeviceId())
                   << "\""
                   << ", Role: " << connection_role
                   << ", Priority: " << connection_priority
                   << ", Details: " << connection_details;
      disconnecting_details_to_requests_map_[connection_details].emplace_back(
          std::move(client_connection_parameters), local_device.GetDeviceId(),
          connection_role, connection_priority);
      break;
  }
}

void SecureChannelImpl::RejectRequestForReason(
    ApiFunctionName api_fn_name,
    mojom::ConnectionAttemptFailureReason reason,
    ClientConnectionParameters* client_connection_parameters) {
  PA_LOG(WARNING) << "SecureChannelImpl::" << api_fn_name << "(): Rejecting "
                  << "request ID: " << client_connection_parameters->id() << " "
                  << "for reason: " << reason;

  client_connection_parameters->SetConnectionAttemptFailed(reason);
}

bool SecureChannelImpl::CheckForInvalidRequest(
    ApiFunctionName api_fn_name,
    ClientConnectionParameters* client_connection_parameters) const {
  if (!client_connection_parameters->IsClientWaitingForResponse()) {
    PA_LOG(ERROR) << "SecureChannelImpl::" << api_fn_name << "(): "
                  << "ConnectionDelegate is not waiting for a response.";
    return true;
  }

  return false;
}

bool SecureChannelImpl::CheckForInvalidInputDevice(
    ApiFunctionName api_fn_name,
    const cryptauth::RemoteDevice& device,
    ClientConnectionParameters* client_connection_parameters,
    bool is_local_device) {
  base::Optional<InvalidRemoteDeviceReason> potential_invalid_reason =
      AddDeviceToCacheIfPossible(api_fn_name, device);
  if (!potential_invalid_reason)
    return false;

  switch (*potential_invalid_reason) {
    case InvalidRemoteDeviceReason::kInvalidPublicKey:
      RejectRequestForReason(api_fn_name,
                             is_local_device
                                 ? mojom::ConnectionAttemptFailureReason::
                                       LOCAL_DEVICE_INVALID_PUBLIC_KEY
                                 : mojom::ConnectionAttemptFailureReason::
                                       REMOTE_DEVICE_INVALID_PUBLIC_KEY,
                             client_connection_parameters);
      break;
    case InvalidRemoteDeviceReason::kInvalidPsk:
      RejectRequestForReason(
          api_fn_name,
          is_local_device
              ? mojom::ConnectionAttemptFailureReason::LOCAL_DEVICE_INVALID_PSK
              : mojom::ConnectionAttemptFailureReason::
                    REMOTE_DEVICE_INVALID_PSK,
          client_connection_parameters);
      break;
  }

  return true;
}

base::Optional<SecureChannelImpl::InvalidRemoteDeviceReason>
SecureChannelImpl::AddDeviceToCacheIfPossible(
    ApiFunctionName api_fn_name,
    const cryptauth::RemoteDevice& device) {
  if (device.public_key.empty()) {
    PA_LOG(WARNING) << "SecureChannelImpl::" << api_fn_name << "(): "
                    << "Provided device has an invalid public key. Cannot "
                    << "process request.";
    return InvalidRemoteDeviceReason::kInvalidPublicKey;
  }

  if (device.persistent_symmetric_key.empty()) {
    PA_LOG(WARNING) << "SecureChannelImpl::" << api_fn_name << "(): "
                    << "Provided device has an invalid PSK. Cannot process "
                    << "request.";
    return InvalidRemoteDeviceReason::kInvalidPsk;
  }

  remote_device_cache_->SetRemoteDevices({device});
  return base::nullopt;
}

std::ostream& operator<<(std::ostream& stream,
                         const SecureChannelImpl::ApiFunctionName& role) {
  switch (role) {
    case SecureChannelImpl::ApiFunctionName::kListenForConnection:
      stream << "ListenForConnectionFromDevice";
      break;
    case SecureChannelImpl::ApiFunctionName::kInitiateConnection:
      stream << "InitiateConnectionToDevice";
      break;
  }
  return stream;
}

}  // namespace secure_channel

}  // namespace chromeos
