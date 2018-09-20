// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_PUBLIC_PROTOCOL_CONNECTION_SERVER_H_
#define API_PUBLIC_PROTOCOL_CONNECTION_SERVER_H_

#include <string>
#include <vector>

#include "api/public/protocol_connection.h"
#include "api/public/server_config.h"
#include "base/error.h"
#include "base/macros.h"

namespace openscreen {

class ProtocolConnectionServer {
 public:
  enum class State {
    kStopped = 0,
    kStarting,
    kRunning,
    kStopping,
    kSuspended,
  };

  class Observer : public ProtocolConnectionObserver {
   public:
    virtual ~Observer() = default;

    // Called when the state becomes kSuspended.
    virtual void OnSuspended() = 0;
  };

  virtual ~ProtocolConnectionServer();

  // Starts the server, listening for new connections on the endpoints in the
  // config object.  Returns true if state() == kStopped and the service will be
  // started, false otherwise.
  virtual bool Start() = 0;

  // Stops the server and frees any resources associated with the server
  // instance.  Returns true if state() != (kStopped|kStopping).
  virtual bool Stop() = 0;

  // NOTE: We need to decide if suspend/resume semantics for QUIC connections
  // are well defined, and if we can resume the server and existing connections
  // in a consistent and useful state.

  // Temporarily stops accepting new connections and sending/receiving data on
  // existing connections.  Any resources associated with existing connections
  // are not freed.
  virtual bool Suspend() = 0;

  // Resumes exchange of data on existing connections and acceptance of new
  // connections.
  virtual bool Resume() = 0;

  // Returns the current state of the listener.
  State state() const { return state_; }

  // Returns the last error reported by this client.
  const Error& last_error() const { return last_error_; }

 protected:
  ProtocolConnectionServer(const ServerConfig& config, Observer* observer);

  ServerConfig config_;
  State state_ = State::kStopped;
  Error last_error_;
  Observer* const observer_;

  DISALLOW_COPY_AND_ASSIGN(ProtocolConnectionServer);
};

}  // namespace openscreen

#endif  // API_PUBLIC_PROTOCOL_CONNECTION_SERVER_H_
