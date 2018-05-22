// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_PENDING_CONNECTION_REQUEST_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_PENDING_CONNECTION_REQUEST_H_

#include <string>
#include <utility>

#include "base/guid.h"
#include "base/macros.h"
#include "chromeos/services/secure_channel/pending_connection_request_delegate.h"
#include "chromeos/services/secure_channel/public/mojom/secure_channel.mojom.h"

namespace chromeos {

namespace secure_channel {

// Encapsulates metadata for a pending request for a connection to a remote
// device. PendingConnectionRequest is templatized so that each derived class
// can specify its own error-handling for connection failures; for instance,
// some derived classes may choose to continue an ongoing connection attempt
// indefinitely, while others may choose to handle connection failures by giving
// up on the request entirely.
template <typename FailureDetailType>
class PendingConnectionRequest {
 public:
  // Extracts |request|'s feature and ConnectionDelegate. This function deletes
  // |request| as part of this process to ensure that it is no longer used after
  // extraction is complete.
  static std::pair<std::string, mojom::ConnectionDelegatePtr> ExtractClientData(
      std::unique_ptr<PendingConnectionRequest<FailureDetailType>> request) {
    return request->ExtractClientData();
  }

  virtual ~PendingConnectionRequest() = default;

  // Handles a failed connection attempt. Derived classes may choose to stop
  // trying to connect after some number of failures.
  virtual void HandleConnectionFailure(FailureDetailType failure_detail) = 0;

  // Note: Request ID is guaranteed to be unique among all requests.
  const std::string& request_id() const { return request_id_; }

 protected:
  PendingConnectionRequest(PendingConnectionRequestDelegate* delegate)
      : delegate_(delegate), request_id_(base::GenerateGUID()) {
    DCHECK(delegate_);
  }

  // Extracts the feature and ConnectionDelegate from this request.
  virtual std::pair<std::string, mojom::ConnectionDelegatePtr>
  ExtractClientData() = 0;

  void NotifyRequestFinishedWithoutConnection(
      PendingConnectionRequestDelegate::FailedConnectionReason reason) {
    delegate_->OnRequestFinishedWithoutConnection(request_id_, reason);
  }

 private:
  PendingConnectionRequestDelegate* delegate_;
  const std::string request_id_;

  DISALLOW_COPY_AND_ASSIGN(PendingConnectionRequest);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_PENDING_CONNECTION_REQUEST_H_
