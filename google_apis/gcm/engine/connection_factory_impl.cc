// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/engine/connection_factory_impl.h"

#include "base/message_loop/message_loop.h"
#include "google_apis/gcm/engine/connection_handler_impl.h"
#include "google_apis/gcm/protocol/mcs.pb.h"
#include "net/base/net_errors.h"
#include "net/http/http_network_session.h"
#include "net/http/http_request_headers.h"
#include "net/proxy/proxy_info.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/client_socket_pool_manager.h"
#include "net/ssl/ssl_config_service.h"

namespace gcm {

namespace {

// The amount of time a Socket read should wait before timing out.
const int kReadTimeoutMs = 30000;  // 30 seconds.

// If a connection is reset after succeeding within this window of time,
// the previous backoff entry is restored (and the connection success is treated
// as if it was transient).
const int kConnectionResetWindowSecs = 10;  // 10 seconds.

// Backoff policy.
const net::BackoffEntry::Policy kConnectionBackoffPolicy = {
  // Number of initial errors (in sequence) to ignore before applying
  // exponential back-off rules.
  0,

  // Initial delay for exponential back-off in ms.
  10000,  // 10 seconds.

  // Factor by which the waiting time will be multiplied.
  2,

  // Fuzzing percentage. ex: 10% will spread requests randomly
  // between 90%-100% of the calculated time.
  0.5,  // 50%.

  // Maximum amount of time we are willing to delay our request in ms.
  1000 * 60 * 5, // 5 minutes.

  // Time to keep an entry from being discarded even when it
  // has no significant state, -1 to never discard.
  -1,

  // Don't use initial delay unless the last request was an error.
  false,
};

}  // namespace

ConnectionFactoryImpl::ConnectionFactoryImpl(
    const GURL& mcs_endpoint,
    scoped_refptr<net::HttpNetworkSession> network_session,
    net::NetLog* net_log)
  : mcs_endpoint_(mcs_endpoint),
    network_session_(network_session),
    net_log_(net_log),
    connecting_(false),
    weak_ptr_factory_(this) {
}

ConnectionFactoryImpl::~ConnectionFactoryImpl() {
}

void ConnectionFactoryImpl::Initialize(
    const BuildLoginRequestCallback& request_builder,
    const ConnectionHandler::ProtoReceivedCallback& read_callback,
    const ConnectionHandler::ProtoSentCallback& write_callback) {
  DCHECK(!connection_handler_);

  previous_backoff_ = CreateBackoffEntry(&kConnectionBackoffPolicy);
  backoff_entry_ = CreateBackoffEntry(&kConnectionBackoffPolicy);
  request_builder_ = request_builder;

  net::NetworkChangeNotifier::AddIPAddressObserver(this);
  net::NetworkChangeNotifier::AddConnectionTypeObserver(this);
  connection_handler_.reset(
      new ConnectionHandlerImpl(
          base::TimeDelta::FromMilliseconds(kReadTimeoutMs),
          read_callback,
          write_callback,
          base::Bind(&ConnectionFactoryImpl::ConnectionHandlerCallback,
                     weak_ptr_factory_.GetWeakPtr())));
}

ConnectionHandler* ConnectionFactoryImpl::GetConnectionHandler() const {
  return connection_handler_.get();
}

void ConnectionFactoryImpl::Connect() {
  DCHECK(connection_handler_);
  DCHECK(!IsEndpointReachable());

  connecting_ = true;
  if (backoff_entry_->ShouldRejectRequest()) {
    DVLOG(1) << "Delaying MCS endpoint connection for "
             << backoff_entry_->GetTimeUntilRelease().InMilliseconds()
             << " milliseconds.";
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&ConnectionFactoryImpl::Connect,
                   weak_ptr_factory_.GetWeakPtr()),
        NextRetryAttempt() - base::TimeTicks::Now());
    return;
  }

  DVLOG(1) << "Attempting connection to MCS endpoint.";
  ConnectImpl();
}

bool ConnectionFactoryImpl::IsEndpointReachable() const {
  return connection_handler_ && connection_handler_->CanSendMessage();
}

void ConnectionFactoryImpl::SignalConnectionReset() {
  if (connecting_)
    return;  // Already attempting to reconnect.

  if (!backoff_reset_time_.is_null() &&
      base::TimeTicks::Now() - backoff_reset_time_ <=
          base::TimeDelta::FromSeconds(kConnectionResetWindowSecs)) {
    backoff_entry_.swap(previous_backoff_);
    backoff_entry_->InformOfRequest(false);
  }
  backoff_reset_time_ = base::TimeTicks();
  previous_backoff_->Reset();
  Connect();
}

base::TimeTicks ConnectionFactoryImpl::NextRetryAttempt() const {
  if (!backoff_entry_)
    return base::TimeTicks();
  return backoff_entry_->GetReleaseTime();
}

void ConnectionFactoryImpl::OnConnectionTypeChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  if (type == net::NetworkChangeNotifier::CONNECTION_NONE)
    return;

  // TODO(zea): implement different backoff/retry policies based on connection
  // type.
  DVLOG(1) << "Connection type changed to " << type << ", resetting backoff.";
  backoff_entry_->Reset();
  // Connect(..) should be retrying with backoff already if a connection is
  // necessary, so no need to call again.
}

void ConnectionFactoryImpl::OnIPAddressChanged() {
  DVLOG(1) << "IP Address changed, resetting backoff.";
  backoff_entry_->Reset();
  // Connect(..) should be retrying with backoff already if a connection is
  // necessary, so no need to call again.
}

void ConnectionFactoryImpl::ConnectImpl() {
  DCHECK(!IsEndpointReachable());

  // TODO(zea): resolve proxies.
  net::ProxyInfo proxy_info;
  proxy_info.UseDirect();
  net::SSLConfig ssl_config;
  network_session_->ssl_config_service()->GetSSLConfig(&ssl_config);

  int status = net::InitSocketHandleForTlsConnect(
      net::HostPortPair::FromURL(mcs_endpoint_),
      network_session_.get(),
      proxy_info,
      ssl_config,
      ssl_config,
      net::kPrivacyModeDisabled,
      net::BoundNetLog::Make(net_log_, net::NetLog::SOURCE_SOCKET),
      &socket_handle_,
      base::Bind(&ConnectionFactoryImpl::OnConnectDone,
                 weak_ptr_factory_.GetWeakPtr()));
  if (status != net::ERR_IO_PENDING)
    OnConnectDone(status);
}

void ConnectionFactoryImpl::InitHandler() {
  // May be null in tests.
  mcs_proto::LoginRequest login_request;
  if (!request_builder_.is_null()) {
    request_builder_.Run(&login_request);
    DCHECK(login_request.IsInitialized());
  }

  connection_handler_->Init(login_request, socket_handle_.PassSocket());
}

scoped_ptr<net::BackoffEntry> ConnectionFactoryImpl::CreateBackoffEntry(
    const net::BackoffEntry::Policy* const policy) {
  return scoped_ptr<net::BackoffEntry>(new net::BackoffEntry(policy));
}

void ConnectionFactoryImpl::OnConnectDone(int result) {
  if (result != net::OK) {
    LOG(ERROR) << "Failed to connect to MCS endpoint with error " << result;
    backoff_entry_->InformOfRequest(false);
    Connect();
    return;
  }

  DVLOG(1) << "MCS endpoint socket connection success, starting handshake.";
  InitHandler();
}

void ConnectionFactoryImpl::ConnectionHandlerCallback(int result) {
  if (result == net::OK) {
    // Handshake succeeded, reset the backoff.
    connecting_ = false;
    backoff_reset_time_ = base::TimeTicks::Now();
    previous_backoff_.swap(backoff_entry_);
    backoff_entry_->Reset();
    return;
  }
  // TODO(zea): Consider how to handle errors that may require some sort of
  // user intervention (login page, etc.).
  LOG(ERROR) << "Connection reset with error " << result;
  backoff_entry_->InformOfRequest(false);
  Connect();
}

}  // namespace gcm
