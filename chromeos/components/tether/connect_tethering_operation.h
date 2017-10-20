// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_CONNECT_TETHERING_OPERATION_H_
#define CHROMEOS_COMPONENTS_TETHER_CONNECT_TETHERING_OPERATION_H_

#include <map>
#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/time/clock.h"
#include "chromeos/components/tether/ble_connection_manager.h"
#include "chromeos/components/tether/message_transfer_operation.h"
#include "components/cryptauth/remote_device.h"

namespace chromeos {

namespace tether {

class MessageWrapper;
class TetherHostResponseRecorder;

// Operation used to request that a tether host share its Internet connection.
// Attempts a connection to the RemoteDevice passed to its constructor and
// notifies observers when the RemoteDevice sends a response.
class ConnectTetheringOperation : public MessageTransferOperation {
 public:
  class Factory {
   public:
    static std::unique_ptr<ConnectTetheringOperation> NewInstance(
        const cryptauth::RemoteDevice& device_to_connect,
        BleConnectionManager* connection_manager,
        TetherHostResponseRecorder* tether_host_response_recorder,
        bool setup_required);

    static void SetInstanceForTesting(Factory* factory);

   protected:
    virtual std::unique_ptr<ConnectTetheringOperation> BuildInstance(
        const cryptauth::RemoteDevice& devices_to_connect,
        BleConnectionManager* connection_manager,
        TetherHostResponseRecorder* tether_host_response_recorder,
        bool setup_required);

   private:
    static Factory* factory_instance_;
  };

  class Observer {
   public:
    virtual void OnConnectTetheringRequestSent(
        const cryptauth::RemoteDevice& remote_device) = 0;
    virtual void OnSuccessfulConnectTetheringResponse(
        const cryptauth::RemoteDevice& remote_device,
        const std::string& ssid,
        const std::string& password) = 0;
    virtual void OnConnectTetheringFailure(
        const cryptauth::RemoteDevice& remote_device,
        ConnectTetheringResponse_ResponseCode error_code) = 0;
  };

  ConnectTetheringOperation(
      const cryptauth::RemoteDevice& device_to_connect,
      BleConnectionManager* connection_manager,
      TetherHostResponseRecorder* tether_host_response_recorder,
      bool setup_required);
  ~ConnectTetheringOperation() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  // MessageTransferOperation:
  void OnDeviceAuthenticated(
      const cryptauth::RemoteDevice& remote_device) override;
  void OnMessageReceived(std::unique_ptr<MessageWrapper> message_wrapper,
                         const cryptauth::RemoteDevice& remote_device) override;
  void OnOperationFinished() override;
  MessageType GetMessageTypeForConnection() override;
  void OnMessageSent(int sequence_number) override;
  uint32_t GetTimeoutSeconds() override;

  void NotifyConnectTetheringRequestSent();
  void NotifyObserversOfSuccessfulResponse(const std::string& ssid,
                                           const std::string& password);
  void NotifyObserversOfConnectionFailure(
      ConnectTetheringResponse_ResponseCode error_code);

 private:
  friend class ConnectTetheringOperationTest;

  void SetClockForTest(std::unique_ptr<base::Clock> clock_for_test);

  // The amount of time this operation will wait for if first time setup is
  // required on the host device.
  static uint32_t kSetupRequiredResponseTimeoutSeconds;

  cryptauth::RemoteDevice remote_device_;
  TetherHostResponseRecorder* tether_host_response_recorder_;
  std::unique_ptr<base::Clock> clock_;
  int connect_message_sequence_number_ = -1;
  bool setup_required_;

  // These values are saved in OnMessageReceived() and returned in
  // OnOperationFinished().
  std::string ssid_to_return_;
  std::string password_to_return_;
  ConnectTetheringResponse_ResponseCode error_code_to_return_;
  base::Time connect_tethering_request_start_time_;

  base::ObserverList<Observer> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(ConnectTetheringOperation);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_CONNECT_TETHERING_OPERATION_H_
