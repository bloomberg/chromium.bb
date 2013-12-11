// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GCM_ENGINE_CONNECTION_FACTORY_IMPL_H_
#define GOOGLE_APIS_GCM_ENGINE_CONNECTION_FACTORY_IMPL_H_

#include "google_apis/gcm/engine/connection_factory.h"

#include "base/memory/weak_ptr.h"
#include "google_apis/gcm/protocol/mcs.pb.h"
#include "net/base/backoff_entry.h"
#include "net/base/network_change_notifier.h"
#include "net/socket/client_socket_handle.h"
#include "url/gurl.h"

namespace net {
class HttpNetworkSession;
class NetLog;
}

namespace gcm {

class ConnectionHandlerImpl;

class GCM_EXPORT ConnectionFactoryImpl :
    public ConnectionFactory,
    public net::NetworkChangeNotifier::ConnectionTypeObserver,
    public net::NetworkChangeNotifier::IPAddressObserver {
 public:
  ConnectionFactoryImpl(
      const GURL& mcs_endpoint,
      scoped_refptr<net::HttpNetworkSession> network_session,
      net::NetLog* net_log);
  virtual ~ConnectionFactoryImpl();

  // ConnectionFactory implementation.
  virtual void Initialize(
      const BuildLoginRequestCallback& request_builder,
      const ConnectionHandler::ProtoReceivedCallback& read_callback,
      const ConnectionHandler::ProtoSentCallback& write_callback) OVERRIDE;
  virtual ConnectionHandler* GetConnectionHandler() const OVERRIDE;
  virtual void Connect() OVERRIDE;
  virtual bool IsEndpointReachable() const OVERRIDE;
  virtual base::TimeTicks NextRetryAttempt() const OVERRIDE;

  // NetworkChangeNotifier observer implementations.
  virtual void OnConnectionTypeChanged(
      net::NetworkChangeNotifier::ConnectionType type) OVERRIDE;
  virtual void OnIPAddressChanged() OVERRIDE;

 protected:
  // Implementation of Connect(..). If not in backoff, uses |login_request_|
  // in attempting a connection/handshake. On connection/handshake failure, goes
  // into backoff.
  // Virtual for testing.
  virtual void ConnectImpl();

  // Helper method for initalizing the connection hander.
  // Virtual for testing.
  virtual void InitHandler();

  // Helper method for creating a backoff entry.
  // Virtual for testing.
  virtual scoped_ptr<net::BackoffEntry> CreateBackoffEntry(
      const net::BackoffEntry::Policy* const policy);

  // Callback for Socket connection completion.
  void OnConnectDone(int result);

 private:
  // ConnectionHandler callback for connection issues.
  void ConnectionHandlerCallback(int result);

  // The MCS endpoint to make connections to.
  const GURL mcs_endpoint_;

  // ---- net:: components for establishing connections. ----
  // Network session for creating new connections.
  const scoped_refptr<net::HttpNetworkSession> network_session_;
  // Net log to use in connection attempts.
  net::NetLog* const net_log_;
  // The handle to the socket for the current connection, if one exists.
  net::ClientSocketHandle socket_handle_;
  // Connection attempt backoff policy.
  scoped_ptr<net::BackoffEntry> backoff_entry_;

  // The current connection handler, if one exists.
  scoped_ptr<ConnectionHandlerImpl> connection_handler_;

  // Builder for generating new login requests.
  BuildLoginRequestCallback request_builder_;

  base::WeakPtrFactory<ConnectionFactoryImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ConnectionFactoryImpl);
};

}  // namespace gcm

#endif  // GOOGLE_APIS_GCM_ENGINE_CONNECTION_FACTORY_IMPL_H_
