// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_CONNECT_TO_DEVICE_OPERATION_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_CONNECT_TO_DEVICE_OPERATION_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "chromeos/services/secure_channel/connect_to_device_operation.h"
#include "chromeos/services/secure_channel/public/cpp/shared/connection_priority.h"

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
          FailureDetailType>::ConnectionFailedCallback failure_callback,
      ConnectionPriority connection_priority)
      : ConnectToDeviceOperation<FailureDetailType>(std::move(success_callback),
                                                    std::move(failure_callback),
                                                    connection_priority) {}

  ~FakeConnectToDeviceOperation() override = default;

  bool canceled() const { return canceled_; }

  const base::Optional<ConnectionPriority>& updated_priority() {
    return updated_priority_;
  }

  void set_destructor_callback(base::OnceClosure destructor_callback) {
    destructor_callback_ = std::move(destructor_callback);
  }

  void set_cancel_callback(base::OnceClosure cancel_callback) {
    cancel_callback_ = std::move(cancel_callback);
  }

  // Make On{Successful|Failed}ConnectionAttempt() public for testing.
  using ConnectToDeviceOperation<
      FailureDetailType>::OnSuccessfulConnectionAttempt;
  using ConnectToDeviceOperation<FailureDetailType>::OnFailedConnectionAttempt;

 private:
  // ConnectToDeviceOperation<FailureDetailType>:
  void CancelInternal() override {
    canceled_ = true;

    if (cancel_callback_)
      std::move(cancel_callback_).Run();
  }

  void UpdateConnectionPriorityInternal(
      ConnectionPriority connection_priority) override {
    updated_priority_ = connection_priority;
  }

  bool canceled_ = false;
  base::Optional<ConnectionPriority> updated_priority_;
  base::OnceClosure destructor_callback_;
  base::OnceClosure cancel_callback_;

  DISALLOW_COPY_AND_ASSIGN(FakeConnectToDeviceOperation);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_CONNECT_TO_DEVICE_OPERATION_H_
