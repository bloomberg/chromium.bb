// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_CONNECT_TO_DEVICE_OPERATION_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_CONNECT_TO_DEVICE_OPERATION_H_

#include "base/callback.h"
#include "base/macros.h"
#include "chromeos/services/secure_channel/connect_to_device_operation.h"

namespace chromeos {

namespace secure_channel {

// Fake ConnectToDeviceOperation implementation
template <typename FailureDetailType>
class FakeConnectToDeviceOperation
    : public ConnectToDeviceOperation<FailureDetailType> {
 public:
  FakeConnectToDeviceOperation(
      typename ConnectToDeviceOperation<
          FailureDetailType>::ConnectionSuccessCallback success_callback,
      typename ConnectToDeviceOperation<
          FailureDetailType>::ConnectionFailedCallback failure_callback)
      : ConnectToDeviceOperation<FailureDetailType>(
            std::move(success_callback),
            std::move(failure_callback)) {}

  ~FakeConnectToDeviceOperation() override = default;

  bool canceled() const { return canceled_; }

  void set_destructor_callback(base::OnceClosure destructor_callback) {
    destructor_callback_ = std::move(destructor_callback);
  }

  // Make On{Successful|Failed}ConnectionAttempt() public for testing.
  using ConnectToDeviceOperation<
      FailureDetailType>::OnSuccessfulConnectionAttempt;
  using ConnectToDeviceOperation<FailureDetailType>::OnFailedConnectionAttempt;

 private:
  // ConnectToDeviceOperation<FailureDetailType>:
  void PerformCancellation() override { canceled_ = true; }

  bool canceled_ = false;
  base::OnceClosure destructor_callback_;

  DISALLOW_COPY_AND_ASSIGN(FakeConnectToDeviceOperation);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_CONNECT_TO_DEVICE_OPERATION_H_
