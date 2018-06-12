// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_BLE_INITIATOR_OPERATION_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_BLE_INITIATOR_OPERATION_H_

#include <memory>

#include "base/macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/services/secure_channel/ble_initiator_failure_type.h"
#include "chromeos/services/secure_channel/connect_to_device_operation.h"
#include "chromeos/services/secure_channel/connect_to_device_operation_base.h"

namespace chromeos {

namespace secure_channel {

// Attempts to connect to a remote device over BLE via the initiator role.
class BleInitiatorOperation
    : public ConnectToDeviceOperationBase<BleInitiatorFailureType> {
 public:
  class Factory {
   public:
    static Factory* Get();
    static void SetFactoryForTesting(Factory* test_factory);
    virtual ~Factory();
    virtual std::unique_ptr<ConnectToDeviceOperation<BleInitiatorFailureType>>
    BuildInstance(ConnectToDeviceOperation<BleInitiatorFailureType>::
                      ConnectionSuccessCallback success_callback,
                  ConnectToDeviceOperation<BleInitiatorFailureType>::
                      ConnectionFailedCallback failure_callback,
                  const DeviceIdPair& device_id_pair,
                  base::OnceClosure destructor_callback,
                  scoped_refptr<base::TaskRunner> task_runner =
                      base::ThreadTaskRunnerHandle::Get());

   private:
    static Factory* test_factory_;
  };

  ~BleInitiatorOperation() override;

 private:
  BleInitiatorOperation(
      ConnectToDeviceOperation<
          BleInitiatorFailureType>::ConnectionSuccessCallback success_callback,
      ConnectToDeviceOperation<
          BleInitiatorFailureType>::ConnectionFailedCallback failure_callback,
      const DeviceIdPair& device_id_pair,
      base::OnceClosure destructor_callback,
      scoped_refptr<base::TaskRunner> task_runner);

  // ConnectToDeviceOperationBase<BleInitiatorFailureType>:
  void AttemptConnectionToDevice() override;
  void CancelConnectionAttemptToDevice() override;

  DISALLOW_COPY_AND_ASSIGN(BleInitiatorOperation);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_BLE_INITIATOR_OPERATION_H_
