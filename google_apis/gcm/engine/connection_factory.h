// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GCM_ENGINE_CONNECTION_FACTORY_H_
#define GOOGLE_APIS_GCM_ENGINE_CONNECTION_FACTORY_H_

#include "base/time/time.h"
#include "google_apis/gcm/base/gcm_export.h"
#include "google_apis/gcm/engine/connection_handler.h"

namespace mcs_proto {
class LoginRequest;
}

namespace gcm {

// Factory for creating a ConnectionHandler and maintaining its connection.
// The factory retains ownership of the ConnectionHandler and will enforce
// backoff policies when attempting connections.
class GCM_EXPORT ConnectionFactory {
 public:
  typedef base::Callback<void(mcs_proto::LoginRequest* login_request)>
      BuildLoginRequestCallback;

  ConnectionFactory();
  virtual ~ConnectionFactory();

  // Initialize the factory, creating a connection handler with a disconnected
  // socket. Should only be called once.
  // Upon connection:
  // |read_callback| will be invoked with the contents of any received protobuf
  // message.
  // |write_callback| will be invoked anytime a message has been successfully
  // sent. Note: this just means the data was sent to the wire, not that the
  // other end received it.
  virtual void Initialize(
      const BuildLoginRequestCallback& request_builder,
      const ConnectionHandler::ProtoReceivedCallback& read_callback,
      const ConnectionHandler::ProtoSentCallback& write_callback) = 0;

  // Get the connection handler for this factory. Initialize(..) must have
  // been called.
  virtual ConnectionHandler* GetConnectionHandler() const = 0;

  // Opens a new connection and initiates login handshake. Upon completion of
  // the handshake, |read_callback| will be invoked with a valid
  // mcs_proto::LoginResponse.
  // Note: Initialize must have already been invoked.
  virtual void Connect() = 0;

  // Whether or not the MCS endpoint is currently reachable with an active
  // connection.
  virtual bool IsEndpointReachable() const = 0;

  // If in backoff, the time at which the next retry will be made. Otherwise,
  // a null time, indicating either no attempt to connect has been made or no
  // backoff is in progress.
  virtual base::TimeTicks NextRetryAttempt() const = 0;
};

}  // namespace gcm

#endif  // GOOGLE_APIS_GCM_ENGINE_CONNECTION_FACTORY_H_
