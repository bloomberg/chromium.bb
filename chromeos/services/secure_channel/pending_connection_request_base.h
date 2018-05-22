// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_PENDING_CONNECTION_REQUEST_BASE_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_PENDING_CONNECTION_REQUEST_BASE_H_

#include <memory>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/components/proximity_auth/logging/logging.h"
#include "chromeos/services/secure_channel/pending_connection_request.h"
#include "chromeos/services/secure_channel/public/mojom/secure_channel.mojom.h"

namespace chromeos {

namespace secure_channel {

// Encapsulates metadata for a pending request for a connection to a remote
// device. Every PendingConnectionRequestBase starts out active (i.e., there
// exists an ongoing attempt to create a connection). The client of this class
// can cancel an active attempt by disconnecting the ConnectionDelegatePtr
// passed PendingConnectionRequestBase's constructor; likewise, a
// PendingConnectionRequestBase can become inactive due to connection failures.
//
// Each connection type should implement its own pending request class deriving
// from PendingConnectionRequestBase.
template <typename FailureDetailType>
class PendingConnectionRequestBase
    : public PendingConnectionRequest<FailureDetailType> {
 public:
  ~PendingConnectionRequestBase() override = default;

 protected:
  PendingConnectionRequestBase(
      const std::string& feature,
      const std::string& readable_request_type_for_logging,
      PendingConnectionRequestDelegate* delegate,
      mojom::ConnectionDelegatePtr connection_delegate_ptr)
      : PendingConnectionRequest<FailureDetailType>(delegate),
        feature_(feature),
        readable_request_type_for_logging_(readable_request_type_for_logging),
        connection_delegate_ptr_(std::move(connection_delegate_ptr)),
        weak_ptr_factory_(this) {
    DCHECK(!feature_.empty());
    DCHECK(connection_delegate_ptr_);

    // If the client disconnects its delegate, the client is signaling that the
    // connection request has been canceled.
    connection_delegate_ptr_.set_connection_error_handler(base::BindOnce(
        &PendingConnectionRequestBase::OnFinishedWithoutConnection,
        weak_ptr_factory_.GetWeakPtr(),
        PendingConnectionRequestDelegate::FailedConnectionReason::
            kRequestCanceledByClient));
  }

  // Derived classes should invoke this function if they would like to give up
  // on the request due to connection failures.
  void StopRequestDueToConnectionFailures(
      mojom::ConnectionAttemptFailureReason failure_reason) {
    if (has_finished_without_connection_) {
      PA_LOG(WARNING) << "PendingConnectionRequest::"
                      << "StopRequestDueToConnectionFailures() invoked after "
                      << "request had already finished without a connection.";
      return;
    }

    connection_delegate_ptr_->OnConnectionAttemptFailure(failure_reason);

    OnFinishedWithoutConnection(PendingConnectionRequestDelegate::
                                    FailedConnectionReason::kRequestFailed);
  }

 private:
  // Make NotifyRequestFinishedWithoutConnection() inaccessible to derived
  // types, which should use StopRequestDueToConnectionFailures() instead.
  using PendingConnectionRequest<
      FailureDetailType>::NotifyRequestFinishedWithoutConnection;

  std::pair<std::string, mojom::ConnectionDelegatePtr> ExtractClientData()
      override {
    return std::make_pair(feature_, std::move(connection_delegate_ptr_));
  }

  void OnFinishedWithoutConnection(
      PendingConnectionRequestDelegate::FailedConnectionReason reason) {
    DCHECK(!has_finished_without_connection_);
    has_finished_without_connection_ = true;

    PA_LOG(INFO) << "Request finished without connection; notifying delegate. "
                 << "Feature: \"" << feature_ << "\""
                 << ", Reason: " << reason << ", Request type: \""
                 << readable_request_type_for_logging_ << "\"";
    NotifyRequestFinishedWithoutConnection(reason);
  }

  const std::string feature_;
  const std::string readable_request_type_for_logging_;
  mojom::ConnectionDelegatePtr connection_delegate_ptr_;

  bool has_finished_without_connection_ = false;

  base::WeakPtrFactory<PendingConnectionRequestBase> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PendingConnectionRequestBase);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_PENDING_CONNECTION_REQUEST_BASE_H_
