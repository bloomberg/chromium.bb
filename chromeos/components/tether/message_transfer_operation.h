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
  MessageTransferOperation(
      const std::vector<cryptauth::RemoteDevice>& devices_to_connect,
      BleConnectionManager* connection_manager);
  virtual ~MessageTransferOperation();

  // Initializes the operation by registering devices with BleConnectionManager.
  void Initialize();

  // BleConnectionManager::Observer:
  void OnSecureChannelStatusChanged(
      const cryptauth::RemoteDevice& remote_device,
      const cryptauth::SecureChannel::Status& old_status,
      const cryptauth::SecureChannel::Status& new_status) override;
  void OnMessageReceived(const cryptauth::RemoteDevice& remote_device,
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

  // Returns true if we should wait for a response from host devices. Note
  // that if ShouldWaitForResponse() returns true and a response is not received
  // within the amount of time returned by GetResponseTimeoutSeconds() after a
  // host device authenticates, that host device will be unregistered.
  virtual bool ShouldWaitForResponse();

  // The number of seconds that we should wait for a response from the host. If
  // ShouldWaitForResponse() returns false, this method is never used.
  virtual uint32_t GetResponseTimeoutSeconds();

  std::vector<cryptauth::RemoteDevice>& remote_devices() {
    return remote_devices_;
  }

 private:
  friend class ConnectTetheringOperationTest;
  friend class DisconnectTetheringOperationTest;
  friend class HostScannerOperationTest;
  friend class MessageTransferOperationTest;

  static uint32_t kMaxConnectionAttemptsPerDevice;

  // The default number of seconds the client should generally wait for a
  // response from the host once an authenticated connection is established.
  // Once this amount of time passes, the connection will be closed. Subclasses
  // of MessageTransferOperation should override GetResponseTimeoutSeconds()
  // if they desire a different duration.
  static uint32_t kDefaultResponseTimeoutSeconds;

  void SetTimerFactoryForTest(
      std::unique_ptr<TimerFactory> timer_factory_for_test);
  void StartResponseTimerForDevice(
      const cryptauth::RemoteDevice& remote_device);
  void StopResponseTimerForDeviceIfRunning(
      const cryptauth::RemoteDevice& remote_device);
  void OnResponseTimeout(const cryptauth::RemoteDevice& remote_device);

  std::vector<cryptauth::RemoteDevice> remote_devices_;
  BleConnectionManager* connection_manager_;
  std::unique_ptr<TimerFactory> timer_factory_;

  bool initialized_;
  std::map<cryptauth::RemoteDevice, uint32_t>
      remote_device_to_num_attempts_map_;
  std::map<cryptauth::RemoteDevice, std::unique_ptr<base::Timer>>
      remote_device_to_timer_map_;
  base::WeakPtrFactory<MessageTransferOperation> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MessageTransferOperation);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_MESSAGE_TRANSFER_OPERATION_H_
