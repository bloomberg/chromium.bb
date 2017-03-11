// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_KEEP_ALIVE_OPERATION_H_
#define CHROMEOS_COMPONENTS_TETHER_KEEP_ALIVE_OPERATION_H_

#include "base/observer_list.h"
#include "chromeos/components/tether/message_transfer_operation.h"

namespace chromeos {

namespace tether {

class BleConnectionManager;

// Operation which sends a keep-alive message to a tether host.
// TODO(khorimoto/hansberry): Change protocol to receive a DeviceStatus update
//                            after sending the KeepAliveTickle message.
class KeepAliveOperation : public MessageTransferOperation {
 public:
  class Factory {
   public:
    static std::unique_ptr<KeepAliveOperation> NewInstance(
        const cryptauth::RemoteDevice& device_to_connect,
        BleConnectionManager* connection_manager);

    static void SetInstanceForTesting(Factory* factory);

   protected:
    virtual std::unique_ptr<KeepAliveOperation> BuildInstance(
        const cryptauth::RemoteDevice& device_to_connect,
        BleConnectionManager* connection_manager);

   private:
    static Factory* factory_instance_;
  };

  class Observer {
   public:
    // TODO(khorimoto): This function should take a DeviceStatus once there is
    //                  keep-alive tickle response.
    virtual void OnOperationFinished() = 0;
  };

  KeepAliveOperation(const cryptauth::RemoteDevice& device_to_connect,
                     BleConnectionManager* connection_manager);
  ~KeepAliveOperation() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  // MessageTransferOperation:
  void OnDeviceAuthenticated(
      const cryptauth::RemoteDevice& remote_device) override;
  void OnOperationFinished() override;
  MessageType GetMessageTypeForConnection() override;

 private:
  friend class KeepAliveOperationTest;

  base::ObserverList<Observer> observer_list_;
  bool has_authenticated_;

  DISALLOW_COPY_AND_ASSIGN(KeepAliveOperation);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_KEEP_ALIVE_OPERATION_H_
