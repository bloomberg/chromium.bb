// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_CONNECTION_ATTEMPT_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_CONNECTION_ATTEMPT_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "chromeos/components/proximity_auth/logging/logging.h"
#include "chromeos/services/secure_channel/connection_attempt_delegate.h"
#include "chromeos/services/secure_channel/pending_connection_request.h"

namespace chromeos {

namespace secure_channel {

class AuthenticatedChannel;

// ConnectionAttempt represents an ongoing attempt to connect to a given device
// over a given medium. Each ConnectionAttempt is comprised of one or
// more PendingConnectionRequests and notifies its delegate when the attempt has
// succeeded or failed.
template <typename FailureDetailType>
class ConnectionAttempt {
 public:
  // Extracts all of the features and ConnectionDelegates owned by |attempt|'s
  // PendingConnectionRequests. This function deletes |attempt| as part of this
  // process to ensure that it is no longer used after extraction is complete.
  static std::vector<std::pair<std::string, mojom::ConnectionDelegatePtr>>
  ExtractClientData(
      std::unique_ptr<ConnectionAttempt<FailureDetailType>> attempt) {
    return attempt->ExtractClientData();
  }

  virtual ~ConnectionAttempt() = default;

  // Note: Attempt ID is guaranteed to be unique.
  const std::string& attempt_id() const { return attempt_id_; }

  // Associates |request| with this attempt. If the attempt succeeds, |request|
  // will be notified of success; on failure, |request| will be notified of a
  // connection failure. Returns whether adding the request was successful.
  bool AddPendingConnectionRequest(
      std::unique_ptr<PendingConnectionRequest<FailureDetailType>> request) {
    if (!request) {
      PA_LOG(ERROR) << "ConnectionAttempt::AddPendingConnectionRequest(): "
                    << "Received invalid request.";
      return false;
    }

    if (has_notified_delegate_) {
      PA_LOG(ERROR) << "ConnectionAttempt::AddPendingConnectionRequest(): "
                    << "Tried to add an additional request,but the attempt had "
                    << "already finished.";
      return false;
    }

    ProcessAddingNewConnectionRequest(std::move(request));
    return true;
  }

 protected:
  ConnectionAttempt(ConnectionAttemptDelegate* delegate)
      : delegate_(delegate), attempt_id_(base::GenerateGUID()) {
    DCHECK(delegate);
  }

  // Derived types should use this function to associate the request with this
  // attempt.
  virtual void ProcessAddingNewConnectionRequest(
      std::unique_ptr<PendingConnectionRequest<FailureDetailType>> request) = 0;

  // Extracts the features and ConnectionDelegates from all child
  // PendingConnectionRequests.
  virtual std::vector<std::pair<std::string, mojom::ConnectionDelegatePtr>>
  ExtractClientData() = 0;

  void OnConnectionAttemptSucceeded(
      std::unique_ptr<AuthenticatedChannel> authenticated_channel) {
    if (has_notified_delegate_) {
      PA_LOG(ERROR) << "ConnectionAttempt::OnConnectionAttemptSucceeded(): "
                    << "Tried to alert delegate of a successful connection, "
                    << "but the attempt had already finished.";
      return;
    }

    has_notified_delegate_ = true;
    delegate_->OnConnectionAttemptSucceeded(attempt_id_,
                                            std::move(authenticated_channel));
  }

  void OnConnectionAttemptFinishedWithoutConnection() {
    if (has_notified_delegate_) {
      PA_LOG(ERROR) << "ConnectionAttempt::"
                    << "OnConnectionAttemptFinishedWithoutConnection(): "
                    << "Tried to alert delegate of a failed connection, "
                    << "but the attempt had already finished.";
      return;
    }

    has_notified_delegate_ = true;
    delegate_->OnConnectionAttemptFinishedWithoutConnection(attempt_id_);
  }

 private:
  ConnectionAttemptDelegate* delegate_;
  const std::string attempt_id_;

  bool has_notified_delegate_ = false;

  DISALLOW_COPY_AND_ASSIGN(ConnectionAttempt);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_CONNECTION_ATTEMPT_H_
