// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/ble_connection_manager.h"

#include "chromeos/components/tether/ble_constants.h"
#include "chromeos/components/tether/timer_factory.h"
#include "components/cryptauth/ble/bluetooth_low_energy_weave_client_connection.h"
#include "components/cryptauth/cryptauth_service.h"
#include "components/proximity_auth/logging/logging.h"
#include "device/bluetooth/bluetooth_uuid.h"

namespace chromeos {

namespace tether {

namespace {
const char kTetherFeature[] = "magic_tether";
}  // namespace

const int64_t BleConnectionManager::kAdvertisingTimeoutMillis = 12000;
const int64_t BleConnectionManager::kFailImmediatelyTimeoutMillis = 0;

// static
std::string BleConnectionManager::MessageTypeToString(
    const MessageType& reason) {
  switch (reason) {
    case MessageType::TETHER_AVAILABILITY_REQUEST:
      return "[TetherAvailabilityRequest]";
    case MessageType::CONNECT_TETHERING_REQUEST:
      return "[ConnectTetheringRequest]";
    case MessageType::KEEP_ALIVE_TICKLE:
      return "[KeepAliveTickle]";
    case MessageType::DISCONNECT_TETHERING_REQUEST:
      return "[DisconnectTetheringRequest]";
    default:
      return "[invalid MessageType]";
  }
}

BleConnectionManager::ConnectionMetadata::ConnectionMetadata(
    const cryptauth::RemoteDevice remote_device,
    std::unique_ptr<base::Timer> timer,
    base::WeakPtr<BleConnectionManager> manager)
    : remote_device_(remote_device),
      connection_attempt_timeout_timer_(std::move(timer)),
      manager_(manager),
      weak_ptr_factory_(this) {}

BleConnectionManager::ConnectionMetadata::~ConnectionMetadata() {}

void BleConnectionManager::ConnectionMetadata::RegisterConnectionReason(
    const MessageType& connection_reason) {
  active_connection_reasons_.insert(connection_reason);
}

void BleConnectionManager::ConnectionMetadata::UnregisterConnectionReason(
    const MessageType& connection_reason) {
  active_connection_reasons_.erase(connection_reason);
}

bool BleConnectionManager::ConnectionMetadata::HasReasonForConnection() const {
  return !active_connection_reasons_.empty();
}

bool BleConnectionManager::ConnectionMetadata::HasEstablishedConnection()
    const {
  return secure_channel_.get();
}

cryptauth::SecureChannel::Status
BleConnectionManager::ConnectionMetadata::GetStatus() const {
  if (connection_attempt_timeout_timer_->IsRunning()) {
    // If the timer is running, a connection attempt is in progress but a
    // channel has not been established.
    return cryptauth::SecureChannel::Status::CONNECTING;
  } else if (!HasEstablishedConnection()) {
    // If there is no timer and a channel has not been established, the channel
    // is disconnected.
    return cryptauth::SecureChannel::Status::DISCONNECTED;
  }

  // If a channel has been established, return its status.
  return secure_channel_->status();
}

void BleConnectionManager::ConnectionMetadata::StartConnectionAttemptTimer(
    bool fail_immediately) {
  DCHECK(!secure_channel_);
  DCHECK(!connection_attempt_timeout_timer_->IsRunning());

  int64_t timeout_millis = fail_immediately ? kFailImmediatelyTimeoutMillis
                                            : kAdvertisingTimeoutMillis;

  connection_attempt_timeout_timer_->Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(timeout_millis),
      base::Bind(&ConnectionMetadata::OnConnectionAttemptTimeout,
                 weak_ptr_factory_.GetWeakPtr()));
}

void BleConnectionManager::ConnectionMetadata::OnConnectionAttemptTimeout() {
  manager_->OnConnectionAttemptTimeout(remote_device_);
}

bool BleConnectionManager::ConnectionMetadata::HasSecureChannel() {
  return secure_channel_ != nullptr;
}

void BleConnectionManager::ConnectionMetadata::SetSecureChannel(
    std::unique_ptr<cryptauth::SecureChannel> secure_channel) {
  DCHECK(!secure_channel_);

  // The connection has succeeded, so cancel the timeout.
  connection_attempt_timeout_timer_->Stop();

  secure_channel_ = std::move(secure_channel);
  secure_channel_->AddObserver(this);
  secure_channel_->Initialize();
}

int BleConnectionManager::ConnectionMetadata::SendMessage(
    const std::string& payload) {
  DCHECK(GetStatus() == cryptauth::SecureChannel::Status::AUTHENTICATED);
  return secure_channel_->SendMessage(std::string(kTetherFeature), payload);
}

void BleConnectionManager::ConnectionMetadata::OnSecureChannelStatusChanged(
    cryptauth::SecureChannel* secure_channel,
    const cryptauth::SecureChannel::Status& old_status,
    const cryptauth::SecureChannel::Status& new_status) {
  DCHECK(secure_channel_.get() == secure_channel);

  if (new_status == cryptauth::SecureChannel::Status::CONNECTING) {
    // BleConnectionManager already broadcasts "disconnected => connecting"
    // status updates when a connection attempt begins, so there is no need to
    // handle this case.
    return;
  }

  // Make a copy of the two statuses. If |secure_channel_.reset()| is called
  // below, the SecureChannel instance will be destroyed and |old_status| and
  // |new_status| may refer to memory which has been deleted.
  const cryptauth::SecureChannel::Status old_status_copy = old_status;
  const cryptauth::SecureChannel::Status new_status_copy = new_status;

  if (new_status == cryptauth::SecureChannel::Status::DISCONNECTED) {
    secure_channel_->RemoveObserver(this);
    secure_channel_.reset();
  }

  manager_->OnSecureChannelStatusChanged(remote_device_, old_status_copy,
                                         new_status_copy);
}

void BleConnectionManager::ConnectionMetadata::OnMessageReceived(
    cryptauth::SecureChannel* secure_channel,
    const std::string& feature,
    const std::string& payload) {
  DCHECK(secure_channel_.get() == secure_channel);
  if (feature != std::string(kTetherFeature)) {
    // If the message received was not a tether feature, ignore it.
    return;
  }

  manager_->SendMessageReceivedEvent(remote_device_, payload);
}

void BleConnectionManager::ConnectionMetadata::OnMessageSent(
    cryptauth::SecureChannel* secure_channel,
    int sequence_number) {
  DCHECK(secure_channel_.get() == secure_channel);
  manager_->SendMessageSentEvent(sequence_number);
}

BleConnectionManager::BleConnectionManager(
    cryptauth::CryptAuthService* cryptauth_service,
    scoped_refptr<device::BluetoothAdapter> adapter,
    const cryptauth::LocalDeviceDataProvider* local_device_data_provider,
    const cryptauth::RemoteBeaconSeedFetcher* remote_beacon_seed_fetcher,
    cryptauth::BluetoothThrottler* bluetooth_throttler)
    : BleConnectionManager(
          cryptauth_service,
          adapter,
          base::MakeUnique<BleScanner>(adapter, local_device_data_provider),
          base::MakeUnique<BleAdvertiser>(adapter,
                                          local_device_data_provider,
                                          remote_beacon_seed_fetcher),
          base::MakeUnique<BleAdvertisementDeviceQueue>(),
          base::MakeUnique<TimerFactory>(),
          bluetooth_throttler) {}

BleConnectionManager::BleConnectionManager(
    cryptauth::CryptAuthService* cryptauth_service,
    scoped_refptr<device::BluetoothAdapter> adapter,
    std::unique_ptr<BleScanner> ble_scanner,
    std::unique_ptr<BleAdvertiser> ble_advertiser,
    std::unique_ptr<BleAdvertisementDeviceQueue> device_queue,
    std::unique_ptr<TimerFactory> timer_factory,
    cryptauth::BluetoothThrottler* bluetooth_throttler)
    : cryptauth_service_(cryptauth_service),
      adapter_(adapter),
      ble_scanner_(std::move(ble_scanner)),
      ble_advertiser_(std::move(ble_advertiser)),
      device_queue_(std::move(device_queue)),
      timer_factory_(std::move(timer_factory)),
      bluetooth_throttler_(bluetooth_throttler),
      has_registered_observer_(false),
      weak_ptr_factory_(this) {}

BleConnectionManager::~BleConnectionManager() {
  if (has_registered_observer_) {
    ble_scanner_->RemoveObserver(this);
  }
}

void BleConnectionManager::RegisterRemoteDevice(
    const cryptauth::RemoteDevice& remote_device,
    const MessageType& connection_reason) {
  if (!has_registered_observer_) {
    ble_scanner_->AddObserver(this);
  }
  has_registered_observer_ = true;

  PA_LOG(INFO) << "Register - Device ID: \""
               << remote_device.GetTruncatedDeviceIdForLogs()
               << "\", Reason: " << MessageTypeToString(connection_reason);

  ConnectionMetadata* connection_metadata =
      GetConnectionMetadata(remote_device);
  if (!connection_metadata) {
    connection_metadata = AddMetadataForDevice(remote_device);
  }

  connection_metadata->RegisterConnectionReason(connection_reason);
  UpdateConnectionAttempts();
}

void BleConnectionManager::UnregisterRemoteDevice(
    const cryptauth::RemoteDevice& remote_device,
    const MessageType& connection_reason) {
  ConnectionMetadata* connection_metadata =
      GetConnectionMetadata(remote_device);
  if (!connection_metadata) {
    PA_LOG(WARNING) << "Tried to unregister device, but was not registered - "
                    << "Device ID: \""
                    << remote_device.GetTruncatedDeviceIdForLogs()
                    << "\", Reason: " << MessageTypeToString(connection_reason);
    return;
  }

  PA_LOG(INFO) << "Unregister - Device ID: \""
               << remote_device.GetTruncatedDeviceIdForLogs()
               << "\", Reason: " << MessageTypeToString(connection_reason);

  connection_metadata->UnregisterConnectionReason(connection_reason);
  if (!connection_metadata->HasReasonForConnection()) {
    cryptauth::SecureChannel::Status status_before_disconnect =
        connection_metadata->GetStatus();
    device_to_metadata_map_.erase(remote_device);
    if (status_before_disconnect ==
        cryptauth::SecureChannel::Status::CONNECTING) {
      StopConnectionAttemptAndMoveToEndOfQueue(remote_device);
    }
    if (status_before_disconnect !=
        cryptauth::SecureChannel::Status::DISCONNECTED) {
      // Send a status update for the disconnection.
      SendSecureChannelStatusChangeEvent(
          remote_device, status_before_disconnect,
          cryptauth::SecureChannel::Status::DISCONNECTED);
    }
  }

  UpdateConnectionAttempts();
}

int BleConnectionManager::SendMessage(
    const cryptauth::RemoteDevice& remote_device,
    const std::string& message) {
  ConnectionMetadata* connection_metadata =
      GetConnectionMetadata(remote_device);
  if (!connection_metadata ||
      connection_metadata->GetStatus() !=
          cryptauth::SecureChannel::Status::AUTHENTICATED) {
    PA_LOG(ERROR) << "SendMessage(): Error - no authenticated channel. "
                  << "Device ID: \""
                  << remote_device.GetTruncatedDeviceIdForLogs() << "\", "
                  << "Message: \"" << message << "\"";
    return -1;
  }

  PA_LOG(INFO) << "SendMessage(): Device ID: \""
               << remote_device.GetTruncatedDeviceIdForLogs() << "\", "
               << "Message: \"" << message << "\"";
  return connection_metadata->SendMessage(message);
}

bool BleConnectionManager::GetStatusForDevice(
    const cryptauth::RemoteDevice& remote_device,
    cryptauth::SecureChannel::Status* status) const {
  ConnectionMetadata* connection_metadata =
      GetConnectionMetadata(remote_device);
  if (!connection_metadata) {
    return false;
  }

  *status = connection_metadata->GetStatus();
  return true;
}

void BleConnectionManager::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void BleConnectionManager::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void BleConnectionManager::OnReceivedAdvertisementFromDevice(
    const std::string& device_address,
    cryptauth::RemoteDevice remote_device) {
  ConnectionMetadata* connection_metadata =
      GetConnectionMetadata(remote_device);
  if (!connection_metadata) {
    // If an advertisement  is received from a device that is not registered,
    // ignore it.
    PA_LOG(WARNING) << "Received an advertisement from a device which is not "
                    << "registered. Bluetooth address: " << device_address
                    << ", Remote Device ID: "
                    << remote_device.GetTruncatedDeviceIdForLogs();
    return;
  }

  if (connection_metadata->HasSecureChannel()) {
    PA_LOG(WARNING) << "Received another advertisement from a registered "
                    << "device which is already being actively communicated "
                    << "with. Bluetooth address: " << device_address
                    << ", Remote Device ID: "
                    << remote_device.GetTruncatedDeviceIdForLogs();
    return;
  }

  PA_LOG(INFO) << "Received advertisement - Device ID: \""
               << remote_device.GetTruncatedDeviceIdForLogs() << "\". "
               << "Starting authentication handshake.";

  // Create a connection to that device.
  std::unique_ptr<cryptauth::Connection> connection =
      cryptauth::weave::BluetoothLowEnergyWeaveClientConnection::Factory::
          NewInstance(remote_device, device_address, adapter_,
                      device::BluetoothUUID(std::string(kGattServerUuid)),
                      bluetooth_throttler_);
  std::unique_ptr<cryptauth::SecureChannel> secure_channel =
      cryptauth::SecureChannel::Factory::NewInstance(std::move(connection),
                                                     cryptauth_service_);
  connection_metadata->SetSecureChannel(std::move(secure_channel));

  // Stop trying to connect to that device, since a connection already exists.
  StopConnectionAttemptAndMoveToEndOfQueue(remote_device);
  UpdateConnectionAttempts();
}

BleConnectionManager::ConnectionMetadata*
BleConnectionManager::GetConnectionMetadata(
    const cryptauth::RemoteDevice& remote_device) const {
  const auto map_iter = device_to_metadata_map_.find(remote_device);
  if (map_iter == device_to_metadata_map_.end()) {
    return nullptr;
  }

  return map_iter->second.get();
}

BleConnectionManager::ConnectionMetadata*
BleConnectionManager::AddMetadataForDevice(
    const cryptauth::RemoteDevice& remote_device) {
  ConnectionMetadata* existing_data = GetConnectionMetadata(remote_device);
  if (existing_data) {
    return existing_data;
  }

  // Create the metadata.
  std::unique_ptr<ConnectionMetadata> metadata =
      base::WrapUnique(new ConnectionMetadata(
          remote_device, timer_factory_->CreateOneShotTimer(),
          weak_ptr_factory_.GetWeakPtr()));
  ConnectionMetadata* metadata_raw_ptr = metadata.get();

  // Add it to the map.
  device_to_metadata_map_.emplace(
      std::pair<cryptauth::RemoteDevice, std::unique_ptr<ConnectionMetadata>>(
          remote_device, std::move(metadata)));

  return metadata_raw_ptr;
}

void BleConnectionManager::UpdateConnectionAttempts() {
  UpdateAdvertisementQueue();

  std::vector<cryptauth::RemoteDevice> should_advertise_to =
      device_queue_->GetDevicesToWhichToAdvertise();
  DCHECK(should_advertise_to.size() <=
         static_cast<size_t>(kMaxConcurrentAdvertisements));

  for (const auto& remote_device : should_advertise_to) {
    ConnectionMetadata* associated_data = GetConnectionMetadata(remote_device);
    if (associated_data->GetStatus() !=
        cryptauth::SecureChannel::Status::CONNECTING) {
      // If there is no active attempt to connect to a device at the front of
      // the queue, start a connection attempt.
      StartConnectionAttempt(remote_device);
    }
  }
}

void BleConnectionManager::UpdateAdvertisementQueue() {
  std::vector<cryptauth::RemoteDevice> devices_for_queue;
  for (const auto& map_entry : device_to_metadata_map_) {
    if (map_entry.second->HasEstablishedConnection()) {
      // If there is already an active connection to the device, there is no
      // need to advertise to the device to bootstrap a connection.
      continue;
    }

    devices_for_queue.push_back(map_entry.first);
  }

  device_queue_->SetDevices(devices_for_queue);
}

void BleConnectionManager::StartConnectionAttempt(
    const cryptauth::RemoteDevice& remote_device) {
  ConnectionMetadata* connection_metadata =
      GetConnectionMetadata(remote_device);
  DCHECK(connection_metadata);

  PA_LOG(INFO) << "Attempting connection - Device ID: \""
               << remote_device.GetTruncatedDeviceIdForLogs() << "\"";

  bool success = ble_scanner_->RegisterScanFilterForDevice(remote_device) &&
                 ble_advertiser_->StartAdvertisingToDevice(remote_device);

  // Start a timer; if a connection is unable to be created before the timer
  // fires, a timeout occurs. Note that if this class is unable to start both
  // the scanner and advertiser successfully (i.e., |success| is |false|), a
  // the connection fails immediately insetad of waiting for a timeout, which
  // has the effect of quickly sending out "disconnected => connecting =>
  // disconnecting" status updates. The timer is used here instead of a special
  // case in order to route all connection failures through the same code path.
  connection_metadata->StartConnectionAttemptTimer(
      !success /* fail_immediately */);

  // Send a "disconnected => connecting" update to alert clients that a
  // connection attempt for |remote_device| is underway.
  SendSecureChannelStatusChangeEvent(
      remote_device, cryptauth::SecureChannel::Status::DISCONNECTED,
      cryptauth::SecureChannel::Status::CONNECTING);
}

void BleConnectionManager::StopConnectionAttemptAndMoveToEndOfQueue(
    const cryptauth::RemoteDevice& remote_device) {
  ble_scanner_->UnregisterScanFilterForDevice(remote_device);
  ble_advertiser_->StopAdvertisingToDevice(remote_device);
  device_queue_->MoveDeviceToEnd(remote_device.GetDeviceId());
}

void BleConnectionManager::OnConnectionAttemptTimeout(
    const cryptauth::RemoteDevice& remote_device) {
  PA_LOG(INFO) << "Connection attempt timeout - Device ID \""
               << remote_device.GetTruncatedDeviceIdForLogs() << "\"";

  StopConnectionAttemptAndMoveToEndOfQueue(remote_device);

  // Send a "connecting => disconnected" update to alert clients that a
  // connection attempt for |remote_device| has failed.
  SendSecureChannelStatusChangeEvent(
      remote_device, cryptauth::SecureChannel::Status::CONNECTING,
      cryptauth::SecureChannel::Status::DISCONNECTED);

  UpdateConnectionAttempts();
}

void BleConnectionManager::OnSecureChannelStatusChanged(
    const cryptauth::RemoteDevice& remote_device,
    const cryptauth::SecureChannel::Status& old_status,
    const cryptauth::SecureChannel::Status& new_status) {
  SendSecureChannelStatusChangeEvent(remote_device, old_status, new_status);
  UpdateConnectionAttempts();
}

void BleConnectionManager::SendMessageReceivedEvent(
    cryptauth::RemoteDevice remote_device,
    std::string payload) {
  PA_LOG(INFO) << "Message received - Device ID: \""
               << remote_device.GetTruncatedDeviceIdForLogs() << "\", "
               << "Message: \"" << payload << "\".";
  for (auto& observer : observer_list_) {
    observer.OnMessageReceived(remote_device, payload);
  }
}

void BleConnectionManager::SendSecureChannelStatusChangeEvent(
    cryptauth::RemoteDevice remote_device,
    cryptauth::SecureChannel::Status old_status,
    cryptauth::SecureChannel::Status new_status) {
  PA_LOG(INFO) << "Status change - Device ID: \""
               << remote_device.GetTruncatedDeviceIdForLogs()
               << "\": " << cryptauth::SecureChannel::StatusToString(old_status)
               << " => "
               << cryptauth::SecureChannel::StatusToString(new_status);
  for (auto& observer : observer_list_) {
    observer.OnSecureChannelStatusChanged(remote_device, old_status,
                                          new_status);
  }
}

void BleConnectionManager::SendMessageSentEvent(int sequence_number) {
  PA_LOG(INFO) << "Message sent successfully; sequence number: "
               << sequence_number;
  for (auto& observer : observer_list_) {
    observer.OnMessageSent(sequence_number);
  }
}

}  // namespace tether

}  // namespace chromeos
