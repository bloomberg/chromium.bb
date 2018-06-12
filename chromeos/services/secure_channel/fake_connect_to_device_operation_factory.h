// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_CONNECT_TO_DEVICE_OPERATION_FACTORY_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_CONNECT_TO_DEVICE_OPERATION_FACTORY_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "chromeos/services/secure_channel/connect_to_device_operation_factory.h"
#include "chromeos/services/secure_channel/fake_connect_to_device_operation_factory.h"

namespace chromeos {

namespace secure_channel {

// Fake ConnectToDeviceOperationFactory implementation.
template <typename FailureDetailType>
class FakeConnectToDeviceOperationFactory
    : public ConnectToDeviceOperationFactory<FailureDetailType> {
 public:
  FakeConnectToDeviceOperationFactory() = default;
  ~FakeConnectToDeviceOperationFactory() override = default;

  FakeConnectToDeviceOperation<FailureDetailType>* last_created_instance() {
    return last_created_instance_;
  }

  size_t num_instances_created() { return num_instances_created_; }

 private:
  // ConnectToDeviceOperationFactory<FailureDetailType>:
  std::unique_ptr<ConnectToDeviceOperation<FailureDetailType>> CreateOperation(
      typename ConnectToDeviceOperation<
          FailureDetailType>::ConnectionSuccessCallback success_callback,
      typename ConnectToDeviceOperation<FailureDetailType>::
          ConnectionFailedCallback failure_callback) override {
    auto instance =
        std::make_unique<FakeConnectToDeviceOperation<FailureDetailType>>(
            std::move(success_callback), std::move(failure_callback));
    last_created_instance_ = instance.get();
    ++num_instances_created_;
    return instance;
  }

  FakeConnectToDeviceOperation<FailureDetailType>* last_created_instance_ =
      nullptr;
  size_t num_instances_created_ = 0u;

  DISALLOW_COPY_AND_ASSIGN(FakeConnectToDeviceOperationFactory);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_CONNECT_TO_DEVICE_OPERATION_FACTORY_H_
