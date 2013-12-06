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
  ConnectionFactory();
  virtual ~ConnectionFactory();

  // Create a new uninitialized connection handler. Should only be called once.
  // The factory will retain ownership of the connection handler.
  // |read_callback| will be invoked with the contents of any received protobuf
  // message.
  // |write_callback| will be invoked anytime a message has been successfully
  // sent. Note: this just means the data was sent to the wire, not that the
  // other end received it.
  virtual ConnectionHandler* BuildConnectionHandler(
      const ConnectionHandler::ProtoReceivedCallback& read_callback,
      const ConnectionHandler::ProtoSentCallback& write_callback) = 0;

  // Opens a new connection for use by the locally owned connection handler
  // (created via BuildConnectionHandler), and initiates login handshake using
  // |login_request|. Upon completion of the handshake, |read_callback|
  // will be invoked with a valid mcs_proto::LoginResponse.
  // Note: BuildConnectionHandler must have already been invoked.
  virtual void Connect(const mcs_proto::LoginRequest& login_request) = 0;

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
