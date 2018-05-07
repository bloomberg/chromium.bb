// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_PENDING_CONNECTION_REQUEST_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_PENDING_CONNECTION_REQUEST_H_

#include "base/guid.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/components/proximity_auth/logging/logging.h"
#include "chromeos/services/secure_channel/pending_connection_request_delegate.h"
#include "chromeos/services/secure_channel/public/mojom/secure_channel.mojom.h"

namespace chromeos {

namespace secure_channel {

// Encapsulates metadata for a pending request for a connection to a remote
// device. Every PendingConnectionRequest starts out active (i.e., there exists
// an ongoing attempt to create a connection). The client of this class can
// cancel an active attempt by disconnecting the ConnectionDelegatePtr passed
// PendingConnectionRequest's constructor; likewise, a PendingConnectionRequest
// can become inactive due to connection failures.
//
// PendingConnectionRequest is templatized so that each derived class can
// specify its own error-handling for connection failures; for instance, some
// derived classes may choose to continue an ongoing connection attempt
// indefinitely, while others may choose to handle connection failures by giving
// up on the request entirely.
template <typename FailureDetailType>
class PendingConnectionRequest {
 public:
  virtual ~PendingConnectionRequest() = default;

  // Handles a failed connection attempt. Derived classes may choose to stop
  // trying to connect after some number of failures.
  virtual void HandleConnectionFailure(FailureDetailType failure_detail) = 0;

  bool is_active() { return is_active_; }

  // Note: Request ID is guaranteed to be unique among all requests.
  const std::string& GetRequestId() {
    static const std::string kRequestId = base::GenerateGUID();
    return kRequestId;
  }

 protected:
  PendingConnectionRequest(const std::string& feature,
                           const std::string& readable_request_type_for_logging,
                           PendingConnectionRequestDelegate* delegate,
                           mojom::ConnectionDelegatePtr connection_delegate_ptr)
      : feature_(feature),
        readable_request_type_for_logging_(readable_request_type_for_logging),
        delegate_(delegate),
        connection_delegate_ptr_(std::move(connection_delegate_ptr)),
        weak_ptr_factory_(this) {
    DCHECK(!feature_.empty());
    DCHECK(delegate_);
    DCHECK(connection_delegate_ptr_);

    // If the client disconnects its delegate, the client is signaling that the
    // connection request has been canceled.
    connection_delegate_ptr_.set_connection_error_handler(
        base::BindOnce(&PendingConnectionRequest::
                           SetInactiveAndNotifyRequestFinishedWithoutConnection,
                       weak_ptr_factory_.GetWeakPtr(),
                       PendingConnectionRequestDelegate::
                           FailedConnectionReason::kRequestCanceledByClient));
  }

  // Derived classes should invoke this function if they would like to give up
  // on the request due to connection failures.
  void StopRequestDueToConnectionFailures() {
    SetInactiveAndNotifyRequestFinishedWithoutConnection(
        PendingConnectionRequestDelegate::FailedConnectionReason::
            kRequestFailed);
  }

 private:
  void SetInactiveAndNotifyRequestFinishedWithoutConnection(
      PendingConnectionRequestDelegate::FailedConnectionReason reason) {
    // If the request is already inactive, there is nothing else to do.
    if (!is_active_)
      return;

    PA_LOG(INFO) << "Request finished without connection; notifying delegate. "
                 << "Feature: \"" << feature_ << "\""
                 << ", Reason: " << reason << ", Request type: \""
                 << readable_request_type_for_logging_ << "\"";

    is_active_ = false;
    delegate_->OnRequestFinishedWithoutConnection(GetRequestId(), reason);
  }

  const std::string feature_;
  const std::string readable_request_type_for_logging_;
  PendingConnectionRequestDelegate* delegate_;
  mojom::ConnectionDelegatePtr connection_delegate_ptr_;

  bool is_active_ = true;
  base::WeakPtrFactory<PendingConnectionRequest> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PendingConnectionRequest);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_PENDING_CONNECTION_REQUEST_H_
