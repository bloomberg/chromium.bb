// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/host_verifier_operation.h"

#include "chromeos/components/proximity_auth/logging/logging.h"

namespace chromeos {

namespace multidevice_setup {

HostVerifierOperation::HostVerifierOperation(Delegate* delegate)
    : delegate_(delegate) {}

HostVerifierOperation::~HostVerifierOperation() = default;

void HostVerifierOperation::CancelOperation() {
  if (result_) {
    PA_LOG(ERROR) << "HostVerifierOperation::CancelOperation(): Tried to "
                  << "cancel operation, but it was already finished. Result: "
                  << *result_;
    NOTREACHED();
  }

  PerformCancelOperation();
  NotifyOperationFinished(Result::kCanceled);
}

void HostVerifierOperation::NotifyOperationFinished(Result result) {
  if (result_) {
    PA_LOG(ERROR) << "HostVerifierOperation::NotifyOperationFinished(): Tried "
                  << "to finish operation, but it was already finished. "
                  << "Result: " << *result_;
  }
  result_ = result;

  delegate_->OnOperationFinished(*result_);
}

std::ostream& operator<<(std::ostream& stream,
                         const HostVerifierOperation::Result& result) {
  switch (result) {
    case HostVerifierOperation::Result::kTimeoutFindingEligibleDevices:
      stream << "[timeout calling FindEligibleDevices()]";
      break;
    case HostVerifierOperation::Result::kErrorCallingFindEligibleDevices:
      stream << "[error calling FindEligibleDevices()]";
      break;
    case HostVerifierOperation::Result::kDeviceToVerifyIsNotEligible:
      stream << "[device to verify was not included in FindEligibleDevices() "
             << "response];";
      break;
    case HostVerifierOperation::Result::kTimeoutFindingConnection:
      stream << "[timeout finding connection]";
      break;
    case HostVerifierOperation::Result::kConnectionAttemptFailed:
      stream << "[connection attempt failed]";
      break;
    case HostVerifierOperation::Result::kConnectionDisconnectedUnexpectedly:
      stream << "[connection disconnected unexpectedly]";
      break;
    case HostVerifierOperation::Result::kTimeoutReceivingResponse:
      stream << "[timeout receiving EnableBetterTogetherResponse]";
      break;
    case HostVerifierOperation::Result::kReceivedInvalidResponse:
      stream << "[received invalid EnableBetterTogetherResponse message]";
      break;
    case HostVerifierOperation::Result::kReceivedErrorResponse:
      stream << "[received EnableBetterTogetherResponse with error]";
      break;
    case HostVerifierOperation::Result::kCanceled:
      stream << "[request canceled]";
      break;
    case HostVerifierOperation::Result::kSuccess:
      stream << "[success]";
      break;
  }
  return stream;
}

}  // namespace multidevice_setup

}  // namespace chromeos
