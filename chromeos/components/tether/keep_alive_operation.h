// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_KEEP_ALIVE_OPERATION_H_
#define CHROMEOS_COMPONENTS_TETHER_KEEP_ALIVE_OPERATION_H_

#include "base/observer_list.h"
#include "base/time/clock.h"
#include "chromeos/components/tether/message_transfer_operation.h"

namespace chromeos {

namespace device_sync {
class DeviceSyncClient;
}  // namespace device_sync

namespace secure_channel {
class SecureChannelClient;
}  // namespace secure_channel

namespace tether {

// Operation which sends a keep-alive message to a tether host and receives an
// update about the host's status.
class KeepAliveOperation : public MessageTransferOperation {
 public:
  class Factory {
   public:
    static std::unique_ptr<KeepAliveOperation> NewInstance(
        multidevice::RemoteDeviceRef device_to_connect,
        device_sync::DeviceSyncClient* device_sync_client,
        secure_channel::SecureChannelClient* secure_channel_client);

    static void SetInstanceForTesting(Factory* factory);

   protected:
    virtual std::unique_ptr<KeepAliveOperation> BuildInstance(
        multidevice::RemoteDeviceRef device_to_connect,
        device_sync::DeviceSyncClient* device_sync_client,
        secure_channel::SecureChannelClient* secure_channel_client);

   private:
    static Factory* factory_instance_;
  };

  class Observer {
   public:
    // |device_status| points to a valid DeviceStatus if the operation completed
    // successfully and is null if the operation was not successful.
    virtual void OnOperationFinished(
        multidevice::RemoteDeviceRef remote_device,
        std::unique_ptr<DeviceStatus> device_status) = 0;
  };

  ~KeepAliveOperation() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  KeepAliveOperation(
      multidevice::RemoteDeviceRef device_to_connect,
      device_sync::DeviceSyncClient* device_sync_client,
      secure_channel::SecureChannelClient* secure_channel_client);

  // MessageTransferOperation:
  void OnDeviceAuthenticated(
      multidevice::RemoteDeviceRef remote_device) override;
  void OnMessageReceived(std::unique_ptr<MessageWrapper> message_wrapper,
                         multidevice::RemoteDeviceRef remote_device) override;
  void OnOperationFinished() override;
  MessageType GetMessageTypeForConnection() override;

  std::unique_ptr<DeviceStatus> device_status_;

 private:
  friend class KeepAliveOperationTest;
  FRIEND_TEST_ALL_PREFIXES(KeepAliveOperationTest,
                           SendsKeepAliveTickleAndReceivesResponse);
  FRIEND_TEST_ALL_PREFIXES(KeepAliveOperationTest, NotifiesObserversOnResponse);
  FRIEND_TEST_ALL_PREFIXES(KeepAliveOperationTest, RecordsResponseDuration);

  void SetClockForTest(base::Clock* clock_for_test);

  multidevice::RemoteDeviceRef remote_device_;
  base::Clock* clock_;
  base::ObserverList<Observer>::Unchecked observer_list_;

  base::Time keep_alive_tickle_request_start_time_;

  DISALLOW_COPY_AND_ASSIGN(KeepAliveOperation);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_KEEP_ALIVE_OPERATION_H_
