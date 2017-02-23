// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_FAKE_BLE_CONNECTION_MANAGER_H_
#define CHROMEOS_COMPONENTS_TETHER_FAKE_BLE_CONNECTION_MANAGER_H_

#include <map>
#include <set>

#include "base/macros.h"
#include "chromeos/components/tether/ble_connection_manager.h"
#include "components/cryptauth/remote_device.h"

namespace chromeos {

namespace tether {

// Test double for BleConnectionManager.
class FakeBleConnectionManager : public BleConnectionManager {
 public:
  FakeBleConnectionManager();
  ~FakeBleConnectionManager() override;

  struct SentMessage {
    cryptauth::RemoteDevice remote_device;
    std::string message;
  };

  void SetDeviceStatus(const cryptauth::RemoteDevice& remote_device,
                       const cryptauth::SecureChannel::Status& status);
  void ReceiveMessage(const cryptauth::RemoteDevice& remote_device,
                      const std::string& payload);
  std::vector<SentMessage>& sent_messages() { return sent_messages_; }

  // BleConnectionManager:
  void RegisterRemoteDevice(const cryptauth::RemoteDevice& remote_device,
                            const MessageType& connection_reason) override;
  void UnregisterRemoteDevice(const cryptauth::RemoteDevice& remote_device,
                              const MessageType& connection_reason) override;
  void SendMessage(const cryptauth::RemoteDevice& remote_device,
                   const std::string& message) override;
  bool GetStatusForDevice(
      const cryptauth::RemoteDevice& remote_device,
      cryptauth::SecureChannel::Status* status) const override;

 private:
  struct StatusAndRegisteredMessageTypes {
    StatusAndRegisteredMessageTypes();
    StatusAndRegisteredMessageTypes(
        const StatusAndRegisteredMessageTypes& other);
    ~StatusAndRegisteredMessageTypes();

    cryptauth::SecureChannel::Status status;
    std::set<MessageType> registered_message_types;
  };

  std::map<cryptauth::RemoteDevice, StatusAndRegisteredMessageTypes>
      device_map_;
  std::vector<SentMessage> sent_messages_;

  DISALLOW_COPY_AND_ASSIGN(FakeBleConnectionManager);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_FAKE_BLE_CONNECTION_MANAGER_H_
