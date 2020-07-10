// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_BLE_INITIATOR_CONNECTION_ATTEMPT_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_BLE_INITIATOR_CONNECTION_ATTEMPT_H_

#include <memory>

#include "base/macros.h"
#include "chromeos/services/secure_channel/ble_initiator_failure_type.h"
#include "chromeos/services/secure_channel/connection_attempt_base.h"

namespace chromeos {

namespace secure_channel {

class BleConnectionManager;

// Attempts to connect to a remote device over BLE via the initiator role.
class BleInitiatorConnectionAttempt
    : public ConnectionAttemptBase<BleInitiatorFailureType> {
 public:
  class Factory {
   public:
    static Factory* Get();
    static void SetFactoryForTesting(Factory* test_factory);
    virtual ~Factory();
    virtual std::unique_ptr<ConnectionAttempt<BleInitiatorFailureType>>
    BuildInstance(BleConnectionManager* ble_connection_manager,
                  ConnectionAttemptDelegate* delegate,
                  const ConnectionAttemptDetails& connection_attempt_details);

   private:
    static Factory* test_factory_;
  };

  ~BleInitiatorConnectionAttempt() override;

 private:
  BleInitiatorConnectionAttempt(
      BleConnectionManager* ble_connection_manager,
      ConnectionAttemptDelegate* delegate,
      const ConnectionAttemptDetails& connection_attempt_details);

  std::unique_ptr<ConnectToDeviceOperation<BleInitiatorFailureType>>
  CreateConnectToDeviceOperation(
      const DeviceIdPair& device_id_pair,
      ConnectionPriority connection_priority,
      ConnectToDeviceOperation<
          BleInitiatorFailureType>::ConnectionSuccessCallback success_callback,
      const ConnectToDeviceOperation<BleInitiatorFailureType>::
          ConnectionFailedCallback& failure_callback) override;

  BleConnectionManager* ble_connection_manager_;

  DISALLOW_COPY_AND_ASSIGN(BleInitiatorConnectionAttempt);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_BLE_INITIATOR_CONNECTION_ATTEMPT_H_
