// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_PUBLIC_PROTOCOL_CONNECTION_CLIENT_H_
#define API_PUBLIC_PROTOCOL_CONNECTION_CLIENT_H_

#include <string>

#include "api/public/protocol_connection.h"
#include "base/error.h"
#include "base/macros.h"

namespace openscreen {

// Embedder's view of the network service that initiates OSP connections to OSP
// receivers.
//
// NOTE: This API closely resembles that for the ProtocolConnectionServer; the
// client currently lacks Suspend(). Consider factoring out a common
// ProtocolConnectionEndpoint when the two APIs are finalized.
class ProtocolConnectionClient {
 public:
  enum class State { kStopped = 0, kStarting, kRunning, kStopping };

  virtual ~ProtocolConnectionClient();

  // Starts the client using the config object.
  // Returns true if state() == kStopped and the service will be started,
  // false otherwise.
  virtual bool Start() = 0;

  // NOTE: Currently we do not support Suspend()/Resume() for the connection
  // client.  Add those if we can define behavior for the OSP protocol and QUIC
  // for those operations.
  // See: https://github.com/webscreens/openscreenprotocol/issues/108

  // Stops listening and cancels any search in progress.
  // Returns true if state() != (kStopped|kStopping).
  virtual bool Stop() = 0;

  // Returns the current state of the listener.
  State state() const { return state_; }

  // Returns the last error reported by this client.
  const Error& last_error() const { return last_error_; }

 protected:
  explicit ProtocolConnectionClient(ProtocolConnectionObserver* observer);

  State state_ = State::kStopped;
  Error last_error_;
  ProtocolConnectionObserver* const observer_;

  DISALLOW_COPY_AND_ASSIGN(ProtocolConnectionClient);
};

}  // namespace openscreen

#endif  // API_PUBLIC_PROTOCOL_CONNECTION_CLIENT_H_
