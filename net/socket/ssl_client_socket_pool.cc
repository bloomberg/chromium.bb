// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/ssl_client_socket_pool.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/http/http_proxy_client_socket.h"
#include "net/http/http_proxy_client_socket_pool.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/socks_client_socket_pool.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/transport_client_socket_pool.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/ssl/ssl_info.h"

namespace net {

SSLSocketParams::SSLSocketParams(
    const scoped_refptr<TransportSocketParams>& direct_params,
    const scoped_refptr<SOCKSSocketParams>& socks_proxy_params,
    const scoped_refptr<HttpProxySocketParams>& http_proxy_params,
    const HostPortPair& host_and_port,
    const SSLConfig& ssl_config,
    PrivacyMode privacy_mode,
    int load_flags,
    bool force_spdy_over_ssl,
    bool want_spdy_over_npn)
    : direct_params_(direct_params),
      socks_proxy_params_(socks_proxy_params),
      http_proxy_params_(http_proxy_params),
      host_and_port_(host_and_port),
      ssl_config_(ssl_config),
      privacy_mode_(privacy_mode),
      load_flags_(load_flags),
      force_spdy_over_ssl_(force_spdy_over_ssl),
      want_spdy_over_npn_(want_spdy_over_npn),
      ignore_limits_(false) {
  if (direct_params_) {
    DCHECK(!socks_proxy_params_);
    DCHECK(!http_proxy_params_);
    ignore_limits_ = direct_params_->ignore_limits();
  } else if (socks_proxy_params_) {
    DCHECK(!http_proxy_params_);
    ignore_limits_ = socks_proxy_params_->ignore_limits();
  } else {
    DCHECK(http_proxy_params_);
    ignore_limits_ = http_proxy_params_->ignore_limits();
  }
}

SSLSocketParams::~SSLSocketParams() {}

SSLSocketParams::ConnectionType SSLSocketParams::GetConnectionType() const {
  if (direct_params_) {
    DCHECK(!socks_proxy_params_);
    DCHECK(!http_proxy_params_);
    return DIRECT;
  }

  if (socks_proxy_params_) {
    DCHECK(!http_proxy_params_);
    return SOCKS_PROXY;
  }

  DCHECK(http_proxy_params_);
  return HTTP_PROXY;
}

const scoped_refptr<TransportSocketParams>&
SSLSocketParams::GetDirectConnectionParams() const {
  DCHECK_EQ(GetConnectionType(), DIRECT);
  return direct_params_;
}

const scoped_refptr<SOCKSSocketParams>&
SSLSocketParams::GetSocksProxyConnectionParams() const {
  DCHECK_EQ(GetConnectionType(), SOCKS_PROXY);
  return socks_proxy_params_;
}

const scoped_refptr<HttpProxySocketParams>&
SSLSocketParams::GetHttpProxyConnectionParams() const {
  DCHECK_EQ(GetConnectionType(), HTTP_PROXY);
  return http_proxy_params_;
}

SSLConnectJobMessenger::SocketAndCallback::SocketAndCallback(
    SSLClientSocket* ssl_socket,
    const base::Closure& job_resumption_callback)
    : socket(ssl_socket), callback(job_resumption_callback) {
}

SSLConnectJobMessenger::SocketAndCallback::~SocketAndCallback() {
}

SSLConnectJobMessenger::SSLConnectJobMessenger(
    const base::Closure& messenger_finished_callback)
    : messenger_finished_callback_(messenger_finished_callback),
      weak_factory_(this) {
}

SSLConnectJobMessenger::~SSLConnectJobMessenger() {
}

void SSLConnectJobMessenger::RemovePendingSocket(SSLClientSocket* ssl_socket) {
  // Sockets do not need to be removed from connecting_sockets_ because
  // OnSSLHandshakeCompleted will do this.
  for (SSLPendingSocketsAndCallbacks::iterator it =
           pending_sockets_and_callbacks_.begin();
       it != pending_sockets_and_callbacks_.end();
       ++it) {
    if (it->socket == ssl_socket) {
      pending_sockets_and_callbacks_.erase(it);
      return;
    }
  }
}

bool SSLConnectJobMessenger::CanProceed(SSLClientSocket* ssl_socket) {
  // If there are no connecting sockets, allow the connection to proceed.
  return connecting_sockets_.empty();
}

void SSLConnectJobMessenger::MonitorConnectionResult(
    SSLClientSocket* ssl_socket) {
  connecting_sockets_.push_back(ssl_socket);
  ssl_socket->SetHandshakeCompletionCallback(
      base::Bind(&SSLConnectJobMessenger::OnSSLHandshakeCompleted,
                 weak_factory_.GetWeakPtr()));
}

void SSLConnectJobMessenger::AddPendingSocket(SSLClientSocket* ssl_socket,
                                              const base::Closure& callback) {
  DCHECK(!connecting_sockets_.empty());
  pending_sockets_and_callbacks_.push_back(
      SocketAndCallback(ssl_socket, callback));
}

void SSLConnectJobMessenger::OnSSLHandshakeCompleted() {
  connecting_sockets_.clear();
  SSLPendingSocketsAndCallbacks temp_list;
  temp_list.swap(pending_sockets_and_callbacks_);
  base::Closure messenger_finished_callback = messenger_finished_callback_;
  messenger_finished_callback.Run();
  RunAllCallbacks(temp_list);
}

void SSLConnectJobMessenger::RunAllCallbacks(
    const SSLPendingSocketsAndCallbacks& pending_sockets_and_callbacks) {
  for (std::vector<SocketAndCallback>::const_iterator it =
           pending_sockets_and_callbacks.begin();
       it != pending_sockets_and_callbacks.end();
       ++it) {
    it->callback.Run();
  }
}

// Timeout for the SSL handshake portion of the connect.
static const int kSSLHandshakeTimeoutInSeconds = 30;

SSLConnectJob::SSLConnectJob(const std::string& group_name,
                             RequestPriority priority,
                             const scoped_refptr<SSLSocketParams>& params,
                             const base::TimeDelta& timeout_duration,
                             TransportClientSocketPool* transport_pool,
                             SOCKSClientSocketPool* socks_pool,
                             HttpProxyClientSocketPool* http_proxy_pool,
                             ClientSocketFactory* client_socket_factory,
                             HostResolver* host_resolver,
                             const SSLClientSocketContext& context,
                             const GetMessengerCallback& get_messenger_callback,
                             Delegate* delegate,
                             NetLog* net_log)
    : ConnectJob(group_name,
                 timeout_duration,
                 priority,
                 delegate,
                 BoundNetLog::Make(net_log, NetLog::SOURCE_CONNECT_JOB)),
      params_(params),
      transport_pool_(transport_pool),
      socks_pool_(socks_pool),
      http_proxy_pool_(http_proxy_pool),
      client_socket_factory_(client_socket_factory),
      host_resolver_(host_resolver),
      context_(context.cert_verifier,
               context.channel_id_service,
               context.transport_security_state,
               context.cert_transparency_verifier,
               (params->privacy_mode() == PRIVACY_MODE_ENABLED
                    ? "pm/" + context.ssl_session_cache_shard
                    : context.ssl_session_cache_shard)),
      io_callback_(
          base::Bind(&SSLConnectJob::OnIOComplete, base::Unretained(this))),
      messenger_(NULL),
      get_messenger_callback_(get_messenger_callback),
      weak_factory_(this) {
}

SSLConnectJob::~SSLConnectJob() {
  if (ssl_socket_.get() && messenger_)
    messenger_->RemovePendingSocket(ssl_socket_.get());
}

LoadState SSLConnectJob::GetLoadState() const {
  switch (next_state_) {
    case STATE_TUNNEL_CONNECT_COMPLETE:
      if (transport_socket_handle_->socket())
        return LOAD_STATE_ESTABLISHING_PROXY_TUNNEL;
      // else, fall through.
    case STATE_TRANSPORT_CONNECT:
    case STATE_TRANSPORT_CONNECT_COMPLETE:
    case STATE_SOCKS_CONNECT:
    case STATE_SOCKS_CONNECT_COMPLETE:
    case STATE_TUNNEL_CONNECT:
      return transport_socket_handle_->GetLoadState();
    case STATE_CREATE_SSL_SOCKET:
    case STATE_CHECK_FOR_RESUME:
    case STATE_SSL_CONNECT:
    case STATE_SSL_CONNECT_COMPLETE:
      return LOAD_STATE_SSL_HANDSHAKE;
    default:
      NOTREACHED();
      return LOAD_STATE_IDLE;
  }
}

void SSLConnectJob::GetAdditionalErrorState(ClientSocketHandle* handle) {
  // Headers in |error_response_info_| indicate a proxy tunnel setup
  // problem. See DoTunnelConnectComplete.
  if (error_response_info_.headers.get()) {
    handle->set_pending_http_proxy_connection(
        transport_socket_handle_.release());
  }
  handle->set_ssl_error_response_info(error_response_info_);
  if (!connect_timing_.ssl_start.is_null())
    handle->set_is_ssl_error(true);
}

void SSLConnectJob::OnIOComplete(int result) {
  int rv = DoLoop(result);
  if (rv != ERR_IO_PENDING)
    NotifyDelegateOfCompletion(rv);  // Deletes |this|.
}

int SSLConnectJob::DoLoop(int result) {
  DCHECK_NE(next_state_, STATE_NONE);

  int rv = result;
  do {
    State state = next_state_;
    next_state_ = STATE_NONE;
    switch (state) {
      case STATE_TRANSPORT_CONNECT:
        DCHECK_EQ(OK, rv);
        rv = DoTransportConnect();
        break;
      case STATE_TRANSPORT_CONNECT_COMPLETE:
        rv = DoTransportConnectComplete(rv);
        break;
      case STATE_SOCKS_CONNECT:
        DCHECK_EQ(OK, rv);
        rv = DoSOCKSConnect();
        break;
      case STATE_SOCKS_CONNECT_COMPLETE:
        rv = DoSOCKSConnectComplete(rv);
        break;
      case STATE_TUNNEL_CONNECT:
        DCHECK_EQ(OK, rv);
        rv = DoTunnelConnect();
        break;
      case STATE_TUNNEL_CONNECT_COMPLETE:
        rv = DoTunnelConnectComplete(rv);
        break;
      case STATE_CREATE_SSL_SOCKET:
        rv = DoCreateSSLSocket();
        break;
      case STATE_CHECK_FOR_RESUME:
        rv = DoCheckForResume();
        break;
      case STATE_SSL_CONNECT:
        DCHECK_EQ(OK, rv);
        rv = DoSSLConnect();
        break;
      case STATE_SSL_CONNECT_COMPLETE:
        rv = DoSSLConnectComplete(rv);
        break;
      default:
        NOTREACHED() << "bad state";
        rv = ERR_FAILED;
        break;
    }
  } while (rv != ERR_IO_PENDING && next_state_ != STATE_NONE);

  return rv;
}

int SSLConnectJob::DoTransportConnect() {
  DCHECK(transport_pool_);

  next_state_ = STATE_TRANSPORT_CONNECT_COMPLETE;
  transport_socket_handle_.reset(new ClientSocketHandle());
  scoped_refptr<TransportSocketParams> direct_params =
      params_->GetDirectConnectionParams();
  return transport_socket_handle_->Init(group_name(),
                                        direct_params,
                                        priority(),
                                        io_callback_,
                                        transport_pool_,
                                        net_log());
}

int SSLConnectJob::DoTransportConnectComplete(int result) {
  if (result == OK)
    next_state_ = STATE_CREATE_SSL_SOCKET;

  return result;
}

int SSLConnectJob::DoSOCKSConnect() {
  DCHECK(socks_pool_);
  next_state_ = STATE_SOCKS_CONNECT_COMPLETE;
  transport_socket_handle_.reset(new ClientSocketHandle());
  scoped_refptr<SOCKSSocketParams> socks_proxy_params =
      params_->GetSocksProxyConnectionParams();
  return transport_socket_handle_->Init(group_name(),
                                        socks_proxy_params,
                                        priority(),
                                        io_callback_,
                                        socks_pool_,
                                        net_log());
}

int SSLConnectJob::DoSOCKSConnectComplete(int result) {
  if (result == OK)
    next_state_ = STATE_CREATE_SSL_SOCKET;

  return result;
}

int SSLConnectJob::DoTunnelConnect() {
  DCHECK(http_proxy_pool_);
  next_state_ = STATE_TUNNEL_CONNECT_COMPLETE;

  transport_socket_handle_.reset(new ClientSocketHandle());
  scoped_refptr<HttpProxySocketParams> http_proxy_params =
      params_->GetHttpProxyConnectionParams();
  return transport_socket_handle_->Init(group_name(),
                                        http_proxy_params,
                                        priority(),
                                        io_callback_,
                                        http_proxy_pool_,
                                        net_log());
}

int SSLConnectJob::DoTunnelConnectComplete(int result) {
  // Extract the information needed to prompt for appropriate proxy
  // authentication so that when ClientSocketPoolBaseHelper calls
  // |GetAdditionalErrorState|, we can easily set the state.
  if (result == ERR_SSL_CLIENT_AUTH_CERT_NEEDED) {
    error_response_info_ = transport_socket_handle_->ssl_error_response_info();
  } else if (result == ERR_PROXY_AUTH_REQUESTED ||
             result == ERR_HTTPS_PROXY_TUNNEL_RESPONSE) {
    StreamSocket* socket = transport_socket_handle_->socket();
    HttpProxyClientSocket* tunnel_socket =
        static_cast<HttpProxyClientSocket*>(socket);
    error_response_info_ = *tunnel_socket->GetConnectResponseInfo();
  }
  if (result < 0)
    return result;
  next_state_ = STATE_CREATE_SSL_SOCKET;
  return result;
}

int SSLConnectJob::DoCreateSSLSocket() {
  next_state_ = STATE_CHECK_FOR_RESUME;

  // Reset the timeout to just the time allowed for the SSL handshake.
  ResetTimer(base::TimeDelta::FromSeconds(kSSLHandshakeTimeoutInSeconds));

  // If the handle has a fresh socket, get its connect start and DNS times.
  // This should always be the case.
  const LoadTimingInfo::ConnectTiming& socket_connect_timing =
      transport_socket_handle_->connect_timing();
  if (!transport_socket_handle_->is_reused() &&
      !socket_connect_timing.connect_start.is_null()) {
    // Overwriting |connect_start| serves two purposes - it adjusts timing so
    // |connect_start| doesn't include dns times, and it adjusts the time so
    // as not to include time spent waiting for an idle socket.
    connect_timing_.connect_start = socket_connect_timing.connect_start;
    connect_timing_.dns_start = socket_connect_timing.dns_start;
    connect_timing_.dns_end = socket_connect_timing.dns_end;
  }

  ssl_socket_ = client_socket_factory_->CreateSSLClientSocket(
      transport_socket_handle_.Pass(),
      params_->host_and_port(),
      params_->ssl_config(),
      context_);

  if (!ssl_socket_->InSessionCache())
    messenger_ = get_messenger_callback_.Run(ssl_socket_->GetSessionCacheKey());

  return OK;
}

int SSLConnectJob::DoCheckForResume() {
  next_state_ = STATE_SSL_CONNECT;

  if (!messenger_)
    return OK;

  if (messenger_->CanProceed(ssl_socket_.get())) {
    messenger_->MonitorConnectionResult(ssl_socket_.get());
    // The SSLConnectJob no longer needs access to the messenger after this
    // point.
    messenger_ = NULL;
    return OK;
  }

  messenger_->AddPendingSocket(ssl_socket_.get(),
                               base::Bind(&SSLConnectJob::ResumeSSLConnection,
                                          weak_factory_.GetWeakPtr()));

  return ERR_IO_PENDING;
}

int SSLConnectJob::DoSSLConnect() {
  next_state_ = STATE_SSL_CONNECT_COMPLETE;

  connect_timing_.ssl_start = base::TimeTicks::Now();

  return ssl_socket_->Connect(io_callback_);
}

int SSLConnectJob::DoSSLConnectComplete(int result) {
  connect_timing_.ssl_end = base::TimeTicks::Now();

  SSLClientSocket::NextProtoStatus status =
      SSLClientSocket::kNextProtoUnsupported;
  std::string proto;
  // GetNextProto will fail and and trigger a NOTREACHED if we pass in a socket
  // that hasn't had SSL_ImportFD called on it. If we get a certificate error
  // here, then we know that we called SSL_ImportFD.
  if (result == OK || IsCertificateError(result))
    status = ssl_socket_->GetNextProto(&proto);

  // If we want spdy over npn, make sure it succeeded.
  if (status == SSLClientSocket::kNextProtoNegotiated) {
    ssl_socket_->set_was_npn_negotiated(true);
    NextProto protocol_negotiated =
        SSLClientSocket::NextProtoFromString(proto);
    ssl_socket_->set_protocol_negotiated(protocol_negotiated);
    // If we negotiated a SPDY version, it must have been present in
    // SSLConfig::next_protos.
    // TODO(mbelshe): Verify this.
    if (protocol_negotiated >= kProtoSPDYMinimumVersion &&
        protocol_negotiated <= kProtoSPDYMaximumVersion) {
      ssl_socket_->set_was_spdy_negotiated(true);
    }
  }
  if (params_->want_spdy_over_npn() && !ssl_socket_->was_spdy_negotiated())
    return ERR_NPN_NEGOTIATION_FAILED;

  // Spdy might be turned on by default, or it might be over npn.
  bool using_spdy = params_->force_spdy_over_ssl() ||
      params_->want_spdy_over_npn();

  if (result == OK ||
      ssl_socket_->IgnoreCertError(result, params_->load_flags())) {
    DCHECK(!connect_timing_.ssl_start.is_null());
    base::TimeDelta connect_duration =
        connect_timing_.ssl_end - connect_timing_.ssl_start;
    if (using_spdy) {
      UMA_HISTOGRAM_CUSTOM_TIMES("Net.SpdyConnectionLatency_2",
                                 connect_duration,
                                 base::TimeDelta::FromMilliseconds(1),
                                 base::TimeDelta::FromMinutes(1),
                                 100);
    }
#if defined(SPDY_PROXY_AUTH_ORIGIN)
    bool using_data_reduction_proxy = params_->host_and_port().Equals(
        HostPortPair::FromURL(GURL(SPDY_PROXY_AUTH_ORIGIN)));
    if (using_data_reduction_proxy) {
      UMA_HISTOGRAM_CUSTOM_TIMES(
          "Net.SSL_Connection_Latency_DataReductionProxy",
          connect_duration,
          base::TimeDelta::FromMilliseconds(1),
          base::TimeDelta::FromMinutes(1),
          100);
    }
#endif

    UMA_HISTOGRAM_CUSTOM_TIMES("Net.SSL_Connection_Latency_2",
                               connect_duration,
                               base::TimeDelta::FromMilliseconds(1),
                               base::TimeDelta::FromMinutes(1),
                               100);

    SSLInfo ssl_info;
    ssl_socket_->GetSSLInfo(&ssl_info);

    UMA_HISTOGRAM_SPARSE_SLOWLY("Net.SSL_CipherSuite",
                                SSLConnectionStatusToCipherSuite(
                                    ssl_info.connection_status));

    if (ssl_info.handshake_type == SSLInfo::HANDSHAKE_RESUME) {
      UMA_HISTOGRAM_CUSTOM_TIMES("Net.SSL_Connection_Latency_Resume_Handshake",
                                 connect_duration,
                                 base::TimeDelta::FromMilliseconds(1),
                                 base::TimeDelta::FromMinutes(1),
                                 100);
    } else if (ssl_info.handshake_type == SSLInfo::HANDSHAKE_FULL) {
      UMA_HISTOGRAM_CUSTOM_TIMES("Net.SSL_Connection_Latency_Full_Handshake",
                                 connect_duration,
                                 base::TimeDelta::FromMilliseconds(1),
                                 base::TimeDelta::FromMinutes(1),
                                 100);
    }

    const std::string& host = params_->host_and_port().host();
    bool is_google =
        host == "google.com" ||
        (host.size() > 11 && host.rfind(".google.com") == host.size() - 11);
    if (is_google) {
      UMA_HISTOGRAM_CUSTOM_TIMES("Net.SSL_Connection_Latency_Google2",
                                 connect_duration,
                                 base::TimeDelta::FromMilliseconds(1),
                                 base::TimeDelta::FromMinutes(1),
                                 100);
      if (ssl_info.handshake_type == SSLInfo::HANDSHAKE_RESUME) {
        UMA_HISTOGRAM_CUSTOM_TIMES("Net.SSL_Connection_Latency_Google_"
                                       "Resume_Handshake",
                                   connect_duration,
                                   base::TimeDelta::FromMilliseconds(1),
                                   base::TimeDelta::FromMinutes(1),
                                   100);
      } else if (ssl_info.handshake_type == SSLInfo::HANDSHAKE_FULL) {
        UMA_HISTOGRAM_CUSTOM_TIMES("Net.SSL_Connection_Latency_Google_"
                                       "Full_Handshake",
                                   connect_duration,
                                   base::TimeDelta::FromMilliseconds(1),
                                   base::TimeDelta::FromMinutes(1),
                                   100);
      }
    }
  }

  if (result == OK || IsCertificateError(result)) {
    SetSocket(ssl_socket_.PassAs<StreamSocket>());
  } else if (result == ERR_SSL_CLIENT_AUTH_CERT_NEEDED) {
    error_response_info_.cert_request_info = new SSLCertRequestInfo;
    ssl_socket_->GetSSLCertRequestInfo(
        error_response_info_.cert_request_info.get());
  }

  return result;
}

void SSLConnectJob::ResumeSSLConnection() {
  DCHECK_EQ(next_state_, STATE_SSL_CONNECT);
  messenger_ = NULL;
  OnIOComplete(OK);
}

SSLConnectJob::State SSLConnectJob::GetInitialState(
    SSLSocketParams::ConnectionType connection_type) {
  switch (connection_type) {
    case SSLSocketParams::DIRECT:
      return STATE_TRANSPORT_CONNECT;
    case SSLSocketParams::HTTP_PROXY:
      return STATE_TUNNEL_CONNECT;
    case SSLSocketParams::SOCKS_PROXY:
      return STATE_SOCKS_CONNECT;
  }
  NOTREACHED();
  return STATE_NONE;
}

int SSLConnectJob::ConnectInternal() {
  next_state_ = GetInitialState(params_->GetConnectionType());
  return DoLoop(OK);
}

SSLClientSocketPool::SSLConnectJobFactory::SSLConnectJobFactory(
    TransportClientSocketPool* transport_pool,
    SOCKSClientSocketPool* socks_pool,
    HttpProxyClientSocketPool* http_proxy_pool,
    ClientSocketFactory* client_socket_factory,
    HostResolver* host_resolver,
    const SSLClientSocketContext& context,
    const SSLConnectJob::GetMessengerCallback& get_messenger_callback,
    NetLog* net_log)
    : transport_pool_(transport_pool),
      socks_pool_(socks_pool),
      http_proxy_pool_(http_proxy_pool),
      client_socket_factory_(client_socket_factory),
      host_resolver_(host_resolver),
      context_(context),
      get_messenger_callback_(get_messenger_callback),
      net_log_(net_log) {
  base::TimeDelta max_transport_timeout = base::TimeDelta();
  base::TimeDelta pool_timeout;
  if (transport_pool_)
    max_transport_timeout = transport_pool_->ConnectionTimeout();
  if (socks_pool_) {
    pool_timeout = socks_pool_->ConnectionTimeout();
    if (pool_timeout > max_transport_timeout)
      max_transport_timeout = pool_timeout;
  }
  if (http_proxy_pool_) {
    pool_timeout = http_proxy_pool_->ConnectionTimeout();
    if (pool_timeout > max_transport_timeout)
      max_transport_timeout = pool_timeout;
  }
  timeout_ = max_transport_timeout +
      base::TimeDelta::FromSeconds(kSSLHandshakeTimeoutInSeconds);
}

SSLClientSocketPool::SSLConnectJobFactory::~SSLConnectJobFactory() {
}

SSLClientSocketPool::SSLClientSocketPool(
    int max_sockets,
    int max_sockets_per_group,
    ClientSocketPoolHistograms* histograms,
    HostResolver* host_resolver,
    CertVerifier* cert_verifier,
    ChannelIDService* channel_id_service,
    TransportSecurityState* transport_security_state,
    CTVerifier* cert_transparency_verifier,
    const std::string& ssl_session_cache_shard,
    ClientSocketFactory* client_socket_factory,
    TransportClientSocketPool* transport_pool,
    SOCKSClientSocketPool* socks_pool,
    HttpProxyClientSocketPool* http_proxy_pool,
    SSLConfigService* ssl_config_service,
    bool enable_ssl_connect_job_waiting,
    NetLog* net_log)
    : transport_pool_(transport_pool),
      socks_pool_(socks_pool),
      http_proxy_pool_(http_proxy_pool),
      base_(this,
            max_sockets,
            max_sockets_per_group,
            histograms,
            ClientSocketPool::unused_idle_socket_timeout(),
            ClientSocketPool::used_idle_socket_timeout(),
            new SSLConnectJobFactory(
                transport_pool,
                socks_pool,
                http_proxy_pool,
                client_socket_factory,
                host_resolver,
                SSLClientSocketContext(cert_verifier,
                                       channel_id_service,
                                       transport_security_state,
                                       cert_transparency_verifier,
                                       ssl_session_cache_shard),
                base::Bind(
                    &SSLClientSocketPool::GetOrCreateSSLConnectJobMessenger,
                    base::Unretained(this)),
                net_log)),
      ssl_config_service_(ssl_config_service),
      enable_ssl_connect_job_waiting_(enable_ssl_connect_job_waiting) {
  if (ssl_config_service_.get())
    ssl_config_service_->AddObserver(this);
  if (transport_pool_)
    base_.AddLowerLayeredPool(transport_pool_);
  if (socks_pool_)
    base_.AddLowerLayeredPool(socks_pool_);
  if (http_proxy_pool_)
    base_.AddLowerLayeredPool(http_proxy_pool_);
}

SSLClientSocketPool::~SSLClientSocketPool() {
  STLDeleteContainerPairSecondPointers(messenger_map_.begin(),
                                       messenger_map_.end());
  if (ssl_config_service_.get())
    ssl_config_service_->RemoveObserver(this);
}

scoped_ptr<ConnectJob> SSLClientSocketPool::SSLConnectJobFactory::NewConnectJob(
    const std::string& group_name,
    const PoolBase::Request& request,
    ConnectJob::Delegate* delegate) const {
  return scoped_ptr<ConnectJob>(new SSLConnectJob(group_name,
                                                  request.priority(),
                                                  request.params(),
                                                  ConnectionTimeout(),
                                                  transport_pool_,
                                                  socks_pool_,
                                                  http_proxy_pool_,
                                                  client_socket_factory_,
                                                  host_resolver_,
                                                  context_,
                                                  get_messenger_callback_,
                                                  delegate,
                                                  net_log_));
}

base::TimeDelta SSLClientSocketPool::SSLConnectJobFactory::ConnectionTimeout()
    const {
  return timeout_;
}

int SSLClientSocketPool::RequestSocket(const std::string& group_name,
                                       const void* socket_params,
                                       RequestPriority priority,
                                       ClientSocketHandle* handle,
                                       const CompletionCallback& callback,
                                       const BoundNetLog& net_log) {
  const scoped_refptr<SSLSocketParams>* casted_socket_params =
      static_cast<const scoped_refptr<SSLSocketParams>*>(socket_params);

  return base_.RequestSocket(group_name, *casted_socket_params, priority,
                             handle, callback, net_log);
}

void SSLClientSocketPool::RequestSockets(
    const std::string& group_name,
    const void* params,
    int num_sockets,
    const BoundNetLog& net_log) {
  const scoped_refptr<SSLSocketParams>* casted_params =
      static_cast<const scoped_refptr<SSLSocketParams>*>(params);

  base_.RequestSockets(group_name, *casted_params, num_sockets, net_log);
}

void SSLClientSocketPool::CancelRequest(const std::string& group_name,
                                        ClientSocketHandle* handle) {
  base_.CancelRequest(group_name, handle);
}

void SSLClientSocketPool::ReleaseSocket(const std::string& group_name,
                                        scoped_ptr<StreamSocket> socket,
                                        int id) {
  base_.ReleaseSocket(group_name, socket.Pass(), id);
}

void SSLClientSocketPool::FlushWithError(int error) {
  base_.FlushWithError(error);
}

void SSLClientSocketPool::CloseIdleSockets() {
  base_.CloseIdleSockets();
}

int SSLClientSocketPool::IdleSocketCount() const {
  return base_.idle_socket_count();
}

int SSLClientSocketPool::IdleSocketCountInGroup(
    const std::string& group_name) const {
  return base_.IdleSocketCountInGroup(group_name);
}

LoadState SSLClientSocketPool::GetLoadState(
    const std::string& group_name, const ClientSocketHandle* handle) const {
  return base_.GetLoadState(group_name, handle);
}

base::DictionaryValue* SSLClientSocketPool::GetInfoAsValue(
    const std::string& name,
    const std::string& type,
    bool include_nested_pools) const {
  base::DictionaryValue* dict = base_.GetInfoAsValue(name, type);
  if (include_nested_pools) {
    base::ListValue* list = new base::ListValue();
    if (transport_pool_) {
      list->Append(transport_pool_->GetInfoAsValue("transport_socket_pool",
                                                   "transport_socket_pool",
                                                   false));
    }
    if (socks_pool_) {
      list->Append(socks_pool_->GetInfoAsValue("socks_pool",
                                               "socks_pool",
                                               true));
    }
    if (http_proxy_pool_) {
      list->Append(http_proxy_pool_->GetInfoAsValue("http_proxy_pool",
                                                    "http_proxy_pool",
                                                    true));
    }
    dict->Set("nested_pools", list);
  }
  return dict;
}

base::TimeDelta SSLClientSocketPool::ConnectionTimeout() const {
  return base_.ConnectionTimeout();
}

ClientSocketPoolHistograms* SSLClientSocketPool::histograms() const {
  return base_.histograms();
}

bool SSLClientSocketPool::IsStalled() const {
  return base_.IsStalled();
}

void SSLClientSocketPool::AddHigherLayeredPool(HigherLayeredPool* higher_pool) {
  base_.AddHigherLayeredPool(higher_pool);
}

void SSLClientSocketPool::RemoveHigherLayeredPool(
    HigherLayeredPool* higher_pool) {
  base_.RemoveHigherLayeredPool(higher_pool);
}

bool SSLClientSocketPool::CloseOneIdleConnection() {
  if (base_.CloseOneIdleSocket())
    return true;
  return base_.CloseOneIdleConnectionInHigherLayeredPool();
}

SSLConnectJobMessenger* SSLClientSocketPool::GetOrCreateSSLConnectJobMessenger(
    const std::string& cache_key) {
  if (!enable_ssl_connect_job_waiting_)
    return NULL;
  MessengerMap::const_iterator it = messenger_map_.find(cache_key);
  if (it == messenger_map_.end()) {
    std::pair<MessengerMap::iterator, bool> iter =
        messenger_map_.insert(MessengerMap::value_type(
            cache_key,
            new SSLConnectJobMessenger(
                base::Bind(&SSLClientSocketPool::DeleteSSLConnectJobMessenger,
                           base::Unretained(this),
                           cache_key))));
    it = iter.first;
  }
  return it->second;
}

void SSLClientSocketPool::DeleteSSLConnectJobMessenger(
    const std::string& cache_key) {
  MessengerMap::iterator it = messenger_map_.find(cache_key);
  CHECK(it != messenger_map_.end());
  delete it->second;
  messenger_map_.erase(it);
}

void SSLClientSocketPool::OnSSLConfigChanged() {
  FlushWithError(ERR_NETWORK_CHANGED);
}

}  // namespace net
