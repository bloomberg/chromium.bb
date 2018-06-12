// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/ble_listener_operation.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"

namespace chromeos {

namespace secure_channel {

// static
BleListenerOperation::Factory* BleListenerOperation::Factory::test_factory_ =
    nullptr;

// static
BleListenerOperation::Factory* BleListenerOperation::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<Factory> factory;
  return factory.get();
}

// static
void BleListenerOperation::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

BleListenerOperation::Factory::~Factory() = default;

std::unique_ptr<ConnectToDeviceOperation<BleListenerFailureType>>
BleListenerOperation::Factory::BuildInstance(
    ConnectToDeviceOperation<BleListenerFailureType>::ConnectionSuccessCallback
        success_callback,
    ConnectToDeviceOperation<BleListenerFailureType>::ConnectionFailedCallback
        failure_callback,
    const DeviceIdPair& device_id_pair,
    base::OnceClosure destructor_callback,
    scoped_refptr<base::TaskRunner> task_runner) {
  return base::WrapUnique(new BleListenerOperation(
      std::move(success_callback), std::move(failure_callback), device_id_pair,
      std::move(destructor_callback), task_runner));
}

BleListenerOperation::~BleListenerOperation() = default;

BleListenerOperation::BleListenerOperation(
    ConnectToDeviceOperation<BleListenerFailureType>::ConnectionSuccessCallback
        success_callback,
    ConnectToDeviceOperation<BleListenerFailureType>::ConnectionFailedCallback
        failure_callback,
    const DeviceIdPair& device_id_pair,
    base::OnceClosure destructor_callback,
    scoped_refptr<base::TaskRunner> task_runner)
    : ConnectToDeviceOperationBase<BleListenerFailureType>(
          std::move(success_callback),
          std::move(failure_callback),
          device_id_pair,
          std::move(destructor_callback),
          task_runner) {}

void BleListenerOperation::AttemptConnectionToDevice() {
  // TODO(khorimoto): Implement.
  NOTIMPLEMENTED();
}

void BleListenerOperation::CancelConnectionAttemptToDevice() {
  // TODO(khorimoto): Implement.
  NOTIMPLEMENTED();
}

}  // namespace secure_channel

}  // namespace chromeos
