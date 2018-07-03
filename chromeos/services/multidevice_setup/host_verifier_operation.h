// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_HOST_VERIFIER_OPERATION_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_HOST_VERIFIER_OPERATION_H_

#include <ostream>

#include "base/macros.h"
#include "base/optional.h"

namespace chromeos {

namespace multidevice_setup {

// Operation for completing the verification step for the current host device.
// A HostVerifierOperation instance is meant to be used for a single
// verification attempt; if verification needs to be retried, a new instance
// should be created for the next attempt.
class HostVerifierOperation {
 public:
  enum class Result {
    kTimeoutFindingEligibleDevices,
    kErrorCallingFindEligibleDevices,
    kDeviceToVerifyIsNotEligible,
    kTimeoutFindingConnection,
    kConnectionAttemptFailed,
    kConnectionDisconnectedUnexpectedly,
    kTimeoutReceivingResponse,
    kReceivedInvalidResponse,
    kReceivedErrorResponse,
    kCanceled,
    kSuccess
  };

  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void OnOperationFinished(Result result) = 0;
  };

  virtual ~HostVerifierOperation();

  // Cancels the operation, triggering a delegate callback with the kCanceled
  // result.
  //
  // It is invalid to call this function after the operation has already
  // completed.
  void CancelOperation();

  // Returns the result of the operation. If the operation has not yet finished,
  // null is returned.
  const base::Optional<Result>& result() const { return result_; }

 protected:
  HostVerifierOperation(Delegate* delegate);

  // Derived types should use this function to cancel the operation, but they
  // should not call NotifyOperationFinished() during cancellation.
  virtual void PerformCancelOperation() = 0;

  void NotifyOperationFinished(Result result);

 private:
  Delegate* delegate_;
  base::Optional<Result> result_;

  DISALLOW_COPY_AND_ASSIGN(HostVerifierOperation);
};

std::ostream& operator<<(std::ostream& stream,
                         const HostVerifierOperation::Result& result);

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_HOST_VERIFIER_OPERATION_H_
