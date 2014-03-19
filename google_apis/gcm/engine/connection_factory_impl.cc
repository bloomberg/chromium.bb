// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/engine/connection_factory_impl.h"

#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
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

// Decides whether the last login was within kConnectionResetWindowSecs of now
// or not.
bool ShouldRestorePreviousBackoff(const base::TimeTicks& login_time,
                                  const base::TimeTicks& now_ticks) {
  return !login_time.is_null() &&
      now_ticks - login_time <=
          base::TimeDelta::FromSeconds(kConnectionResetWindowSecs);
}

}  // namespace

ConnectionFactoryImpl::ConnectionFactoryImpl(
    const GURL& mcs_endpoint,
    const net::BackoffEntry::Policy& backoff_policy,
    scoped_refptr<net::HttpNetworkSession> network_session,
    net::NetLog* net_log)
  : mcs_endpoint_(mcs_endpoint),
    backoff_policy_(backoff_policy),
    network_session_(network_session),
    bound_net_log_(
        net::BoundNetLog::Make(net_log, net::NetLog::SOURCE_SOCKET)),
    pac_request_(NULL),
    connecting_(false),
    logging_in_(false),
    weak_ptr_factory_(this) {
}

ConnectionFactoryImpl::~ConnectionFactoryImpl() {
  if (pac_request_) {
    network_session_->proxy_service()->CancelPacRequest(pac_request_);
    pac_request_ = NULL;
  }
}

void ConnectionFactoryImpl::Initialize(
    const BuildLoginRequestCallback& request_builder,
    const ConnectionHandler::ProtoReceivedCallback& read_callback,
    const ConnectionHandler::ProtoSentCallback& write_callback) {
  DCHECK(!connection_handler_);

  previous_backoff_ = CreateBackoffEntry(&backoff_policy_);
  backoff_entry_ = CreateBackoffEntry(&backoff_policy_);
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

  connecting_ = true;
  if (backoff_entry_->ShouldRejectRequest()) {
    DVLOG(1) << "Delaying MCS endpoint connection for "
             << backoff_entry_->GetTimeUntilRelease().InMilliseconds()
             << " milliseconds.";
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&ConnectionFactoryImpl::Connect,
                   weak_ptr_factory_.GetWeakPtr()),
        backoff_entry_->GetTimeUntilRelease());
    return;
  }

  DVLOG(1) << "Attempting connection to MCS endpoint.";
  ConnectImpl();
}

bool ConnectionFactoryImpl::IsEndpointReachable() const {
  return connection_handler_ &&
      connection_handler_->CanSendMessage() &&
      !connecting_;
}

void ConnectionFactoryImpl::SignalConnectionReset(
    ConnectionResetReason reason) {
  // A failure can trigger multiple resets, so no need to do anything if a
  // connection is already in progress.
  if (connecting_)
    return;

  UMA_HISTOGRAM_ENUMERATION("GCM.ConnectionResetReason",
                            reason,
                            CONNECTION_RESET_COUNT);
  if (!last_login_time_.is_null()) {
    UMA_HISTOGRAM_CUSTOM_TIMES("GCM.ConnectionUpTime",
                               NowTicks() - last_login_time_,
                               base::TimeDelta::FromSeconds(1),
                               base::TimeDelta::FromHours(24),
                               50);
    // |last_login_time_| will be reset below, before attempting the new
    // connection.
  }

  CloseSocket();

  if (logging_in_) {
    // Failures prior to login completion just reuse the existing backoff entry.
    logging_in_ = false;
    backoff_entry_->InformOfRequest(false);
  } else if (reason == LOGIN_FAILURE ||
             ShouldRestorePreviousBackoff(last_login_time_, NowTicks())) {
    // Failures due to login, or within the reset window of a login, restore
    // the backoff entry that was saved off at login completion time.
    backoff_entry_.swap(previous_backoff_);
    backoff_entry_->InformOfRequest(false);
  } else {
    // We shouldn't be in backoff in thise case.
    DCHECK_EQ(0, backoff_entry_->failure_count());
  }

  // At this point the last login time has been consumed or deemed irrelevant,
  // reset it.
  last_login_time_ = base::TimeTicks();

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
  DCHECK(connecting_);
  DCHECK(!socket_handle_.socket());

  int status = network_session_->proxy_service()->ResolveProxy(
      mcs_endpoint_,
      &proxy_info_,
      base::Bind(&ConnectionFactoryImpl::OnProxyResolveDone,
                 weak_ptr_factory_.GetWeakPtr()),
      &pac_request_,
      bound_net_log_);
  if (status != net::ERR_IO_PENDING)
    OnProxyResolveDone(status);
}

void ConnectionFactoryImpl::InitHandler() {
  // May be null in tests.
  mcs_proto::LoginRequest login_request;
  if (!request_builder_.is_null()) {
    request_builder_.Run(&login_request);
    DCHECK(login_request.IsInitialized());
  }

  connection_handler_->Init(login_request, socket_handle_.socket());
}

scoped_ptr<net::BackoffEntry> ConnectionFactoryImpl::CreateBackoffEntry(
    const net::BackoffEntry::Policy* const policy) {
  return scoped_ptr<net::BackoffEntry>(new net::BackoffEntry(policy));
}

base::TimeTicks ConnectionFactoryImpl::NowTicks() {
  return base::TimeTicks::Now();
}

void ConnectionFactoryImpl::OnConnectDone(int result) {
  if (result != net::OK) {
    // If the connection fails, try another proxy.
    result = ReconsiderProxyAfterError(result);
    // ReconsiderProxyAfterError either returns an error (in which case it is
    // not reconsidering a proxy) or returns ERR_IO_PENDING if it is considering
    // another proxy.
    DCHECK_NE(result, net::OK);
    if (result == net::ERR_IO_PENDING)
      return;  // Proxy reconsideration pending. Return.
    LOG(ERROR) << "Failed to connect to MCS endpoint with error " << result;
    UMA_HISTOGRAM_BOOLEAN("GCM.ConnectionSuccessRate", false);
    CloseSocket();
    backoff_entry_->InformOfRequest(false);
    UMA_HISTOGRAM_SPARSE_SLOWLY("GCM.ConnectionFailureErrorCode", result);
    Connect();
    return;
  }

  UMA_HISTOGRAM_BOOLEAN("GCM.ConnectionSuccessRate", true);
  ReportSuccessfulProxyConnection();

  connecting_ = false;
  logging_in_ = true;
  DVLOG(1) << "MCS endpoint socket connection success, starting login.";
  InitHandler();
}

void ConnectionFactoryImpl::ConnectionHandlerCallback(int result) {
  DCHECK(!connecting_);
  if (result != net::OK) {
    // TODO(zea): Consider how to handle errors that may require some sort of
    // user intervention (login page, etc.).
    UMA_HISTOGRAM_SPARSE_SLOWLY("GCM.ConnectionDisconnectErrorCode", result);
    SignalConnectionReset(SOCKET_FAILURE);
    return;
  }

  // Handshake complete, reset backoff. If the login failed with an error,
  // the client should invoke SignalConnectionReset(LOGIN_FAILURE), which will
  // restore the previous backoff.
  DVLOG(1) << "Handshake complete.";
  last_login_time_ = NowTicks();
  previous_backoff_.swap(backoff_entry_);
  backoff_entry_->Reset();
  logging_in_ = false;
}

// This has largely been copied from
// HttpStreamFactoryImpl::Job::DoResolveProxyComplete. This should be
// refactored into some common place.
void ConnectionFactoryImpl::OnProxyResolveDone(int status) {
  pac_request_ = NULL;
  DVLOG(1) << "Proxy resolution status: " << status;

  DCHECK_NE(status, net::ERR_IO_PENDING);
  if (status == net::OK) {
    // Remove unsupported proxies from the list.
    proxy_info_.RemoveProxiesWithoutScheme(
        net::ProxyServer::SCHEME_DIRECT |
        net::ProxyServer::SCHEME_HTTP | net::ProxyServer::SCHEME_HTTPS |
        net::ProxyServer::SCHEME_SOCKS4 | net::ProxyServer::SCHEME_SOCKS5);

    if (proxy_info_.is_empty()) {
      // No proxies/direct to choose from. This happens when we don't support
      // any of the proxies in the returned list.
      status = net::ERR_NO_SUPPORTED_PROXIES;
    }
  }

  if (status != net::OK) {
    // Failed to resolve proxy. Retry later.
    OnConnectDone(status);
    return;
  }

  DVLOG(1) << "Resolved proxy with PAC:" << proxy_info_.ToPacString();

  net::SSLConfig ssl_config;
  network_session_->ssl_config_service()->GetSSLConfig(&ssl_config);
  status = net::InitSocketHandleForTlsConnect(
      net::HostPortPair::FromURL(mcs_endpoint_),
      network_session_.get(),
      proxy_info_,
      ssl_config,
      ssl_config,
      net::kPrivacyModeDisabled,
      bound_net_log_,
      &socket_handle_,
      base::Bind(&ConnectionFactoryImpl::OnConnectDone,
                 weak_ptr_factory_.GetWeakPtr()));
  if (status != net::ERR_IO_PENDING)
    OnConnectDone(status);
}

// This has largely been copied from
// HttpStreamFactoryImpl::Job::ReconsiderProxyAfterError. This should be
// refactored into some common place.
// This method reconsiders the proxy on certain errors. If it does reconsider
// a proxy it always returns ERR_IO_PENDING and posts a call to
// OnProxyResolveDone with the result of the reconsideration.
int ConnectionFactoryImpl::ReconsiderProxyAfterError(int error) {
  DCHECK(!pac_request_);
  DCHECK_NE(error, net::OK);
  DCHECK_NE(error, net::ERR_IO_PENDING);
  // A failure to resolve the hostname or any error related to establishing a
  // TCP connection could be grounds for trying a new proxy configuration.
  //
  // Why do this when a hostname cannot be resolved?  Some URLs only make sense
  // to proxy servers.  The hostname in those URLs might fail to resolve if we
  // are still using a non-proxy config.  We need to check if a proxy config
  // now exists that corresponds to a proxy server that could load the URL.
  //
  switch (error) {
    case net::ERR_PROXY_CONNECTION_FAILED:
    case net::ERR_NAME_NOT_RESOLVED:
    case net::ERR_INTERNET_DISCONNECTED:
    case net::ERR_ADDRESS_UNREACHABLE:
    case net::ERR_CONNECTION_CLOSED:
    case net::ERR_CONNECTION_TIMED_OUT:
    case net::ERR_CONNECTION_RESET:
    case net::ERR_CONNECTION_REFUSED:
    case net::ERR_CONNECTION_ABORTED:
    case net::ERR_TIMED_OUT:
    case net::ERR_TUNNEL_CONNECTION_FAILED:
    case net::ERR_SOCKS_CONNECTION_FAILED:
    // This can happen in the case of trying to talk to a proxy using SSL, and
    // ending up talking to a captive portal that supports SSL instead.
    case net::ERR_PROXY_CERTIFICATE_INVALID:
    // This can happen when trying to talk SSL to a non-SSL server (Like a
    // captive portal).
    case net::ERR_SSL_PROTOCOL_ERROR:
      break;
    case net::ERR_SOCKS_CONNECTION_HOST_UNREACHABLE:
      // Remap the SOCKS-specific "host unreachable" error to a more
      // generic error code (this way consumers like the link doctor
      // know to substitute their error page).
      //
      // Note that if the host resolving was done by the SOCKS5 proxy, we can't
      // differentiate between a proxy-side "host not found" versus a proxy-side
      // "address unreachable" error, and will report both of these failures as
      // ERR_ADDRESS_UNREACHABLE.
      return net::ERR_ADDRESS_UNREACHABLE;
    default:
      return error;
  }

  net::SSLConfig ssl_config;
  network_session_->ssl_config_service()->GetSSLConfig(&ssl_config);
  if (proxy_info_.is_https() && ssl_config.send_client_cert) {
    network_session_->ssl_client_auth_cache()->Remove(
        proxy_info_.proxy_server().host_port_pair());
  }

  int status = network_session_->proxy_service()->ReconsiderProxyAfterError(
      mcs_endpoint_, &proxy_info_,
      base::Bind(&ConnectionFactoryImpl::OnProxyResolveDone,
                 weak_ptr_factory_.GetWeakPtr()),
      &pac_request_,
      bound_net_log_);
  if (status == net::OK || status == net::ERR_IO_PENDING) {
    CloseSocket();
  } else {
    // If ReconsiderProxyAfterError() failed synchronously, it means
    // there was nothing left to fall-back to, so fail the transaction
    // with the last connection error we got.
    status = error;
  }

  // If there is new proxy info, post OnProxyResolveDone to retry it. Otherwise,
  // if there was an error falling back, fail synchronously.
  if (status == net::OK) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&ConnectionFactoryImpl::OnProxyResolveDone,
                   weak_ptr_factory_.GetWeakPtr(), status));
    status = net::ERR_IO_PENDING;
  }
  return status;
}

void ConnectionFactoryImpl::ReportSuccessfulProxyConnection() {
  if (network_session_ && network_session_->proxy_service())
    network_session_->proxy_service()->ReportSuccess(proxy_info_);
}

void ConnectionFactoryImpl::CloseSocket() {
  // The connection handler needs to be reset, else it'll attempt to keep using
  // the destroyed socket.
  if (connection_handler_)
    connection_handler_->Reset();

  if (socket_handle_.socket() && socket_handle_.socket()->IsConnected())
    socket_handle_.socket()->Disconnect();
  socket_handle_.Reset();
}

}  // namespace gcm
