// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/ble_initiator_operation.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"

namespace chromeos {

namespace secure_channel {

// static
BleInitiatorOperation::Factory* BleInitiatorOperation::Factory::test_factory_ =
    nullptr;

// static
BleInitiatorOperation::Factory* BleInitiatorOperation::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<Factory> factory;
  return factory.get();
}

// static
void BleInitiatorOperation::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

BleInitiatorOperation::Factory::~Factory() = default;

std::unique_ptr<ConnectToDeviceOperation<BleInitiatorFailureType>>
BleInitiatorOperation::Factory::BuildInstance(
    ConnectToDeviceOperation<BleInitiatorFailureType>::ConnectionSuccessCallback
        success_callback,
    ConnectToDeviceOperation<BleInitiatorFailureType>::ConnectionFailedCallback
        failure_callback,
    const DeviceIdPair& device_id_pair,
    base::OnceClosure destructor_callback,
    scoped_refptr<base::TaskRunner> task_runner) {
  return base::WrapUnique(new BleInitiatorOperation(
      std::move(success_callback), std::move(failure_callback), device_id_pair,
      std::move(destructor_callback), task_runner));
}

BleInitiatorOperation::~BleInitiatorOperation() = default;

BleInitiatorOperation::BleInitiatorOperation(
    ConnectToDeviceOperation<BleInitiatorFailureType>::ConnectionSuccessCallback
        success_callback,
    ConnectToDeviceOperation<BleInitiatorFailureType>::ConnectionFailedCallback
        failure_callback,
    const DeviceIdPair& device_id_pair,
    base::OnceClosure destructor_callback,
    scoped_refptr<base::TaskRunner> task_runner)
    : ConnectToDeviceOperationBase<BleInitiatorFailureType>(
          std::move(success_callback),
          std::move(failure_callback),
          device_id_pair,
          std::move(destructor_callback),
          task_runner) {}

void BleInitiatorOperation::AttemptConnectionToDevice() {
  // TODO(khorimoto): Implement.
  NOTIMPLEMENTED();
}

void BleInitiatorOperation::CancelConnectionAttemptToDevice() {
  // TODO(khorimoto): Implement.
  NOTIMPLEMENTED();
}

}  // namespace secure_channel

}  // namespace chromeos
