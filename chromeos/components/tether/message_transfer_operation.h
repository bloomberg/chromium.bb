// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_MESSAGE_TRANSFER_OPERATION_H_
#define CHROMEOS_COMPONENTS_TETHER_MESSAGE_TRANSFER_OPERATION_H_

#include <map>
#include <vector>

#include "base/macros.h"
#include "base/timer/timer.h"
#include "chromeos/components/tether/ble_connection_manager.h"

namespace chromeos {

namespace tether {

class MessageWrapper;
class TimerFactory;

// Abstract base class used for operations which send and/or receive messages
// from remote devices.
class MessageTransferOperation : public BleConnectionManager::Observer {
 public:
  // The number of times to attempt to connect to a device without receiving any
  // response before giving up. When a connection to a device is attempted, a
  // BLE discovery session listens for advertisements from the remote device as
  // the first step of the connection; if no advertisement is picked up, it is
  // likely that the remote device is not nearby or is not currently responding
  // to Instant Tethering requests.
  static const uint32_t kMaxEmptyScansPerDevice;

  // The number of times to attempt a GATT connection to a device, after a BLE
  // discovery session has already detected a nearby device. GATT connections
  // may fail for a variety of reasons, but most failures are ephemeral. Thus,
  // more connection attempts are allowed in such cases since it is likely that
  // a subsequent attempt will succeed. See https://crbug.com/805218.
  static const uint32_t kMaxGattConnectionAttemptsPerDevice;

  MessageTransferOperation(
      const std::vector<cryptauth::RemoteDevice>& devices_to_connect,
      BleConnectionManager* connection_manager);
  virtual ~MessageTransferOperation();

  // Initializes the operation by registering devices with BleConnectionManager.
  void Initialize();

  // BleConnectionManager::Observer:
  void OnSecureChannelStatusChanged(
      const std::string& device_id,
      const cryptauth::SecureChannel::Status& old_status,
      const cryptauth::SecureChannel::Status& new_status,
      BleConnectionManager::StateChangeDetail status_change_detail) override;
  void OnMessageReceived(const std::string& device_id,
                         const std::string& payload) override;
  void OnMessageSent(int sequence_number) override {}

 protected:
  // Unregisters |remote_device| for the MessageType returned by
  // GetMessageTypeForConnection().
  void UnregisterDevice(const cryptauth::RemoteDevice& remote_device);

  // Sends |message_wrapper|'s message to |remote_device| and returns the
  // associated message's sequence number.
  int SendMessageToDevice(const cryptauth::RemoteDevice& remote_device,
                          std::unique_ptr<MessageWrapper> message_wrapper);

  // Callback executed whena device is authenticated (i.e., it is in a state
  // which allows messages to be sent/received). Should be overridden by derived
  // classes which intend to send a message to |remote_device| as soon as an
  // authenticated channel has been established to that device.
  virtual void OnDeviceAuthenticated(
      const cryptauth::RemoteDevice& remote_device) {}

  // Callback executed when a tether protocol message is received. Should be
  // overriden by derived classes which intend to handle messages received from
  // |remote_device|.
  virtual void OnMessageReceived(
      std::unique_ptr<MessageWrapper> message_wrapper,
      const cryptauth::RemoteDevice& remote_device) {}

  // Callback executed when the operation has started (i.e., in Initialize()).
  virtual void OnOperationStarted() {}

  // Callback executed when the operation has finished (i.e., when all devices
  // have been unregistered).
  virtual void OnOperationFinished() {}

  // Returns the type of message that this operation intends to send.
  virtual MessageType GetMessageTypeForConnection() = 0;

  // The number of seconds that this operation should wait before unregistering
  // a device after it has been authenticated if it has not been explicitly
  // unregistered. If ShouldOperationUseTimeout() returns false, this method is
  // never used.
  virtual uint32_t GetTimeoutSeconds();

  std::vector<cryptauth::RemoteDevice>& remote_devices() {
    return remote_devices_;
  }

 private:
  friend class ConnectTetheringOperationTest;
  friend class DisconnectTetheringOperationTest;
  friend class HostScannerOperationTest;
  friend class MessageTransferOperationTest;

  // The default number of seconds an operation should wait before a timeout
  // occurs. Once this amount of time passes, the connection will be closed.
  // Classes deriving from MessageTransferOperation should override
  // GetTimeoutSeconds() if they desire a different duration.
  static constexpr const uint32_t kDefaultTimeoutSeconds = 10;

  struct ConnectAttemptCounts {
    uint32_t empty_scan_attempts = 0;
    uint32_t gatt_connection_attempts = 0;
  };

  void HandleDeviceDisconnection(
      const cryptauth::RemoteDevice& remote_device,
      BleConnectionManager::StateChangeDetail status_change_detail);
  void StartTimerForDevice(const cryptauth::RemoteDevice& remote_device);
  void StopTimerForDeviceIfRunning(
      const cryptauth::RemoteDevice& remote_device);
  void OnTimeout(const cryptauth::RemoteDevice& remote_device);
  cryptauth::RemoteDevice* GetRemoteDevice(const std::string& device_id);

  void SetTimerFactoryForTest(
      std::unique_ptr<TimerFactory> timer_factory_for_test);

  std::vector<cryptauth::RemoteDevice> remote_devices_;
  BleConnectionManager* connection_manager_;
  std::unique_ptr<TimerFactory> timer_factory_;

  bool initialized_ = false;
  bool shutting_down_ = false;
  MessageType message_type_for_connection_;
  std::map<cryptauth::RemoteDevice, ConnectAttemptCounts>
      remote_device_to_attempts_map_;
  std::map<cryptauth::RemoteDevice, std::unique_ptr<base::Timer>>
      remote_device_to_timer_map_;
  base::WeakPtrFactory<MessageTransferOperation> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MessageTransferOperation);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_MESSAGE_TRANSFER_OPERATION_H_
