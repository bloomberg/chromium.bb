// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_PUBLIC_PROTOCOL_CONNECTION_SERVER_H_
#define API_PUBLIC_PROTOCOL_CONNECTION_SERVER_H_

#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "api/public/message_demuxer.h"
#include "api/public/protocol_connection.h"
#include "api/public/server_config.h"
#include "base/error.h"
#include "base/ip_address.h"
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

  class Observer : public ProtocolConnectionServiceObserver {
   public:
    virtual ~Observer() = default;

    // Called when the state becomes kSuspended.
    virtual void OnSuspended() = 0;

    virtual void OnIncomingConnection(
        std::unique_ptr<ProtocolConnection>&& connection) = 0;
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

  virtual void RunTasks() = 0;

  // Synchronously open a new connection to an endpoint identified by
  // |endpoint_id|.  Returns nullptr if it can't be completed synchronously
  // (e.g. there are no existing open connections to that endpoint).
  virtual std::unique_ptr<ProtocolConnection> CreateProtocolConnection(
      uint64_t endpoint_id) = 0;

  // Returns the current state of the listener.
  State state() const { return state_; }

  // Returns the last error reported by this client.
  const Error& last_error() const { return last_error_; }

 protected:
  explicit ProtocolConnectionServer(MessageDemuxer* demuxer,
                                    Observer* observer);

  State state_ = State::kStopped;
  Error last_error_;
  MessageDemuxer* const demuxer_;
  Observer* const observer_;

  DISALLOW_COPY_AND_ASSIGN(ProtocolConnectionServer);
};

std::ostream& operator<<(std::ostream& os,
                         ProtocolConnectionServer::State state);

}  // namespace openscreen

#endif  // API_PUBLIC_PROTOCOL_CONNECTION_SERVER_H_
