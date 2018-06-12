// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_BLE_LISTENER_OPERATION_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_BLE_LISTENER_OPERATION_H_

#include <memory>

#include "base/macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/services/secure_channel/ble_listener_failure_type.h"
#include "chromeos/services/secure_channel/connect_to_device_operation.h"
#include "chromeos/services/secure_channel/connect_to_device_operation_base.h"

namespace chromeos {

namespace secure_channel {

// Attempts to connect to a remote device over BLE via the listener role.
class BleListenerOperation
    : public ConnectToDeviceOperationBase<BleListenerFailureType> {
 public:
  class Factory {
   public:
    static Factory* Get();
    static void SetFactoryForTesting(Factory* test_factory);
    virtual ~Factory();
    virtual std::unique_ptr<ConnectToDeviceOperation<BleListenerFailureType>>
    BuildInstance(
        ConnectToDeviceOperation<
            BleListenerFailureType>::ConnectionSuccessCallback success_callback,
        ConnectToDeviceOperation<
            BleListenerFailureType>::ConnectionFailedCallback failure_callback,
        const DeviceIdPair& device_id_pair,
        base::OnceClosure destructor_callback,
        scoped_refptr<base::TaskRunner> task_runner =
            base::ThreadTaskRunnerHandle::Get());

   private:
    static Factory* test_factory_;
  };

  ~BleListenerOperation() override;

 private:
  BleListenerOperation(
      ConnectToDeviceOperation<
          BleListenerFailureType>::ConnectionSuccessCallback success_callback,
      ConnectToDeviceOperation<BleListenerFailureType>::ConnectionFailedCallback
          failure_callback,
      const DeviceIdPair& device_id_pair,
      base::OnceClosure destructor_callback,
      scoped_refptr<base::TaskRunner> task_runner);

  // ConnectToDeviceOperationBase<BleListenerFailureType>:
  void AttemptConnectionToDevice() override;
  void CancelConnectionAttemptToDevice() override;

  DISALLOW_COPY_AND_ASSIGN(BleListenerOperation);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_BLE_LISTENER_OPERATION_H_
