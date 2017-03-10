// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_KEEP_ALIVE_OPERATION_H_
#define CHROMEOS_COMPONENTS_TETHER_KEEP_ALIVE_OPERATION_H_

#include "chromeos/components/tether/message_transfer_operation.h"

namespace chromeos {

namespace tether {

class BleConnectionManager;

// Operation which sends a keep-alive message to a tether host.
// TODO(khorimoto/hansberry): Consider changing protocol to receive a
//                            DeviceStatus update after sending the
//                            KeepAliveTickle message.
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
        const cryptauth::RemoteDevice& devices_to_connect,
        BleConnectionManager* connection_manager);

   private:
    static Factory* factory_instance_;
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
  MessageType GetMessageTypeForConnection() override;

 private:
  friend class KeepAliveOperationTest;

  DISALLOW_COPY_AND_ASSIGN(KeepAliveOperation);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_KEEP_ALIVE_OPERATION_H_
