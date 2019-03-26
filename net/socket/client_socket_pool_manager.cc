// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/client_socket_pool_manager.h"

#include <memory>

#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "net/base/features.h"
#include "net/base/load_flags.h"
#include "net/http/http_proxy_connect_job.h"
#include "net/http/http_stream_factory.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/client_socket_pool.h"
#include "net/socket/socks_connect_job.h"
#include "net/socket/ssl_connect_job.h"
#include "net/socket/transport_connect_job.h"
#include "net/ssl/ssl_config.h"

namespace net {

namespace {

// Limit of sockets of each socket pool.
int g_max_sockets_per_pool[] = {
  256,  // NORMAL_SOCKET_POOL
  256   // WEBSOCKET_SOCKET_POOL
};

static_assert(base::size(g_max_sockets_per_pool) ==
                  HttpNetworkSession::NUM_SOCKET_POOL_TYPES,
              "max sockets per pool length mismatch");

// Default to allow up to 6 connections per host. Experiment and tuning may
// try other values (greater than 0).  Too large may cause many problems, such
// as home routers blocking the connections!?!?  See http://crbug.com/12066.
//
// WebSocket connections are long-lived, and should be treated differently
// than normal other connections. Use a limit of 255, so the limit for wss will
// be the same as the limit for ws. Also note that Firefox uses a limit of 200.
// See http://crbug.com/486800
int g_max_sockets_per_group[] = {
    6,   // NORMAL_SOCKET_POOL
    255  // WEBSOCKET_SOCKET_POOL
};

static_assert(base::size(g_max_sockets_per_group) ==
                  HttpNetworkSession::NUM_SOCKET_POOL_TYPES,
              "max sockets per group length mismatch");

// The max number of sockets to allow per proxy server.  This applies both to
// http and SOCKS proxies.  See http://crbug.com/12066 and
// http://crbug.com/44501 for details about proxy server connection limits.
int g_max_sockets_per_proxy_server[] = {
  kDefaultMaxSocketsPerProxyServer,  // NORMAL_SOCKET_POOL
  kDefaultMaxSocketsPerProxyServer   // WEBSOCKET_SOCKET_POOL
};

static_assert(base::size(g_max_sockets_per_proxy_server) ==
                  HttpNetworkSession::NUM_SOCKET_POOL_TYPES,
              "max sockets per proxy server length mismatch");

// The meat of the implementation for the InitSocketHandleForHttpRequest,
// InitSocketHandleForRawConnect and PreconnectSocketsForHttpRequest methods.
//
// DO NOT ADD ANY ARGUMENTS TO THIS METHOD.
//
// TODO(https://crbug.com/921369) In order to resolve longstanding issues
// related to pooling distinguishable sockets together, reduce the arguments to
// just those that are used to populate |connection_group|, and those used to
// locate the socket pool to use.
scoped_refptr<ClientSocketPool::SocketParams> CreateSocketParamsAndGetGroupId(
    ClientSocketPoolManager::SocketGroupType group_type,
    const HostPortPair& endpoint,
    const ProxyInfo& proxy_info,
    // This argument should be removed.
    const SSLConfig& ssl_config_for_origin,
    // This argument should be removed.
    const SSLConfig& ssl_config_for_proxy,
    // This argument should be removed.
    bool force_tunnel,
    PrivacyMode privacy_mode,
    // TODO(https://crbug.com/912727):  This argument should be removed.
    const OnHostResolutionCallback& resolution_callback,
    ClientSocketPool::GroupId* connection_group) {
  scoped_refptr<HttpProxySocketParams> http_proxy_params;
  scoped_refptr<SOCKSSocketParams> socks_params;

  const bool using_ssl = group_type == ClientSocketPoolManager::SSL_GROUP;

  // Build the string used to uniquely identify connections of this type.
  // Determine the host and port to connect to.
  DCHECK(!endpoint.IsEmpty());
  ClientSocketPool::SocketType socket_type =
      ClientSocketPool::SocketType::kHttp;

  if (group_type == ClientSocketPoolManager::FTP_GROUP) {
    // Combining FTP with forced SPDY over SSL would be a "path to madness".
    // Make sure we never do that.
    DCHECK(!using_ssl);
    socket_type = ClientSocketPool::SocketType::kFtp;
  }
  if (using_ssl) {
    if (!ssl_config_for_origin.version_interference_probe) {
      socket_type = ClientSocketPool::SocketType::kSsl;
    } else {
      socket_type = ClientSocketPool::SocketType::kSslVersionInterferenceProbe;
    }
  }

  *connection_group = ClientSocketPool::GroupId(
      endpoint, socket_type, privacy_mode == PRIVACY_MODE_ENABLED);

  if (!proxy_info.is_direct()) {
    ProxyServer proxy_server = proxy_info.proxy_server();
    scoped_refptr<TransportSocketParams> proxy_tcp_params =
        base::MakeRefCounted<TransportSocketParams>(
            proxy_server.host_port_pair(), resolution_callback);

    if (proxy_info.is_http() || proxy_info.is_https() || proxy_info.is_quic()) {
      scoped_refptr<SSLSocketParams> ssl_params;
      if (!proxy_info.is_http()) {
        // Set ssl_params, and unset proxy_tcp_params
        ssl_params = base::MakeRefCounted<SSLSocketParams>(
            std::move(proxy_tcp_params), nullptr, nullptr,
            proxy_server.host_port_pair(), ssl_config_for_proxy,
            PRIVACY_MODE_DISABLED);
        proxy_tcp_params = nullptr;
      }

      http_proxy_params = base::MakeRefCounted<HttpProxySocketParams>(
          std::move(proxy_tcp_params), std::move(ssl_params),
          proxy_info.is_quic(), endpoint, proxy_server.is_trusted_proxy(),
          force_tunnel || using_ssl,
          NetworkTrafficAnnotationTag(proxy_info.traffic_annotation()));
    } else {
      DCHECK(proxy_info.is_socks());
      socks_params = base::MakeRefCounted<SOCKSSocketParams>(
          std::move(proxy_tcp_params),
          proxy_server.scheme() == ProxyServer::SCHEME_SOCKS5, endpoint,
          NetworkTrafficAnnotationTag(proxy_info.traffic_annotation()));
    }
  }

  // Deal with SSL - which layers on top of any given proxy.
  if (using_ssl) {
    scoped_refptr<TransportSocketParams> ssl_tcp_params;
    if (proxy_info.is_direct()) {
      ssl_tcp_params = base::MakeRefCounted<TransportSocketParams>(
          endpoint, resolution_callback);
    }
    scoped_refptr<SSLSocketParams> ssl_params =
        base::MakeRefCounted<SSLSocketParams>(
            std::move(ssl_tcp_params), std::move(socks_params),
            std::move(http_proxy_params), endpoint, ssl_config_for_origin,
            privacy_mode);
    return ClientSocketPool::SocketParams::CreateFromSSLSocketParams(
        std::move(ssl_params));
  }

  if (proxy_info.is_http() || proxy_info.is_https()) {
    return ClientSocketPool::SocketParams::CreateFromHttpProxySocketParams(
        std::move(http_proxy_params));
  }

  if (proxy_info.is_socks()) {
    return ClientSocketPool::SocketParams::CreateFromSOCKSSocketParams(
        std::move(socks_params));
  }

  DCHECK(proxy_info.is_direct());
  scoped_refptr<TransportSocketParams> tcp_params =
      base::MakeRefCounted<TransportSocketParams>(endpoint,
                                                  resolution_callback);
  return ClientSocketPool::SocketParams::CreateFromTransportSocketParams(
      std::move(tcp_params));
}

int InitSocketPoolHelper(
    ClientSocketPoolManager::SocketGroupType group_type,
    const HostPortPair& endpoint,
    int request_load_flags,
    RequestPriority request_priority,
    HttpNetworkSession* session,
    const ProxyInfo& proxy_info,
    const SSLConfig& ssl_config_for_origin,
    const SSLConfig& ssl_config_for_proxy,
    bool force_tunnel,
    PrivacyMode privacy_mode,
    const SocketTag& socket_tag,
    const NetLogWithSource& net_log,
    int num_preconnect_streams,
    ClientSocketHandle* socket_handle,
    HttpNetworkSession::SocketPoolType socket_pool_type,
    const OnHostResolutionCallback& resolution_callback,
    CompletionOnceCallback callback,
    const ClientSocketPool::ProxyAuthCallback& proxy_auth_callback) {
  bool using_ssl = group_type == ClientSocketPoolManager::SSL_GROUP;
  HostPortPair origin_host_port = endpoint;

  if (!using_ssl && session->params().testing_fixed_http_port != 0) {
    origin_host_port.set_port(session->params().testing_fixed_http_port);
  } else if (using_ssl && session->params().testing_fixed_https_port != 0) {
    origin_host_port.set_port(session->params().testing_fixed_https_port);
  }

  ClientSocketPool::GroupId connection_group;
  scoped_refptr<ClientSocketPool::SocketParams> socket_params =
      CreateSocketParamsAndGetGroupId(
          group_type, origin_host_port, proxy_info, ssl_config_for_origin,
          ssl_config_for_proxy, force_tunnel, privacy_mode, resolution_callback,
          &connection_group);

  ClientSocketPool* pool =
      session->GetSocketPool(socket_pool_type, proxy_info.proxy_server());
  ClientSocketPool::RespectLimits respect_limits =
      ClientSocketPool::RespectLimits::ENABLED;
  if ((request_load_flags & LOAD_IGNORE_LIMITS) != 0)
    respect_limits = ClientSocketPool::RespectLimits::DISABLED;

  if (num_preconnect_streams) {
    pool->RequestSockets(connection_group, std::move(socket_params),
                         num_preconnect_streams, net_log);
    return OK;
  }

  return socket_handle->Init(
      connection_group, std::move(socket_params), request_priority, socket_tag,
      respect_limits, std::move(callback), proxy_auth_callback, pool, net_log);
}

}  // namespace

ClientSocketPoolManager::ClientSocketPoolManager() = default;
ClientSocketPoolManager::~ClientSocketPoolManager() = default;

// static
int ClientSocketPoolManager::max_sockets_per_pool(
    HttpNetworkSession::SocketPoolType pool_type) {
  DCHECK_LT(pool_type, HttpNetworkSession::NUM_SOCKET_POOL_TYPES);
  return g_max_sockets_per_pool[pool_type];
}

// static
void ClientSocketPoolManager::set_max_sockets_per_pool(
    HttpNetworkSession::SocketPoolType pool_type,
    int socket_count) {
  DCHECK_LT(0, socket_count);
  DCHECK_GT(1000, socket_count);  // Sanity check.
  DCHECK_LT(pool_type, HttpNetworkSession::NUM_SOCKET_POOL_TYPES);
  g_max_sockets_per_pool[pool_type] = socket_count;
  DCHECK_GE(g_max_sockets_per_pool[pool_type],
            g_max_sockets_per_group[pool_type]);
}

// static
int ClientSocketPoolManager::max_sockets_per_group(
    HttpNetworkSession::SocketPoolType pool_type) {
  DCHECK_LT(pool_type, HttpNetworkSession::NUM_SOCKET_POOL_TYPES);
  return g_max_sockets_per_group[pool_type];
}

// static
void ClientSocketPoolManager::set_max_sockets_per_group(
    HttpNetworkSession::SocketPoolType pool_type,
    int socket_count) {
  DCHECK_LT(0, socket_count);
  // The following is a sanity check... but we should NEVER be near this value.
  DCHECK_GT(100, socket_count);
  DCHECK_LT(pool_type, HttpNetworkSession::NUM_SOCKET_POOL_TYPES);
  g_max_sockets_per_group[pool_type] = socket_count;

  DCHECK_GE(g_max_sockets_per_pool[pool_type],
            g_max_sockets_per_group[pool_type]);
  DCHECK_GE(g_max_sockets_per_proxy_server[pool_type],
            g_max_sockets_per_group[pool_type]);
}

// static
int ClientSocketPoolManager::max_sockets_per_proxy_server(
    HttpNetworkSession::SocketPoolType pool_type) {
  DCHECK_LT(pool_type, HttpNetworkSession::NUM_SOCKET_POOL_TYPES);
  return g_max_sockets_per_proxy_server[pool_type];
}

// static
void ClientSocketPoolManager::set_max_sockets_per_proxy_server(
    HttpNetworkSession::SocketPoolType pool_type,
    int socket_count) {
  DCHECK_LT(0, socket_count);
  DCHECK_GT(100, socket_count);  // Sanity check.
  DCHECK_LT(pool_type, HttpNetworkSession::NUM_SOCKET_POOL_TYPES);
  // Assert this case early on. The max number of sockets per group cannot
  // exceed the max number of sockets per proxy server.
  DCHECK_LE(g_max_sockets_per_group[pool_type], socket_count);
  g_max_sockets_per_proxy_server[pool_type] = socket_count;
}

// static
base::TimeDelta ClientSocketPoolManager::unused_idle_socket_timeout(
    HttpNetworkSession::SocketPoolType pool_type) {
  return base::TimeDelta::FromSeconds(base::GetFieldTrialParamByFeatureAsInt(
      net::features::kNetUnusedIdleSocketTimeout,
      "unused_idle_socket_timeout_seconds", 10));
}

int InitSocketHandleForHttpRequest(
    ClientSocketPoolManager::SocketGroupType group_type,
    const HostPortPair& endpoint,
    int request_load_flags,
    RequestPriority request_priority,
    HttpNetworkSession* session,
    const ProxyInfo& proxy_info,
    const SSLConfig& ssl_config_for_origin,
    const SSLConfig& ssl_config_for_proxy,
    PrivacyMode privacy_mode,
    const SocketTag& socket_tag,
    const NetLogWithSource& net_log,
    ClientSocketHandle* socket_handle,
    const OnHostResolutionCallback& resolution_callback,
    CompletionOnceCallback callback,
    const ClientSocketPool::ProxyAuthCallback& proxy_auth_callback) {
  DCHECK(socket_handle);
  return InitSocketPoolHelper(
      group_type, endpoint, request_load_flags, request_priority, session,
      proxy_info, ssl_config_for_origin, ssl_config_for_proxy,
      /*force_tunnel=*/false, privacy_mode, socket_tag, net_log, 0,
      socket_handle, HttpNetworkSession::NORMAL_SOCKET_POOL,
      resolution_callback, std::move(callback), proxy_auth_callback);
}

int InitSocketHandleForWebSocketRequest(
    ClientSocketPoolManager::SocketGroupType group_type,
    const HostPortPair& endpoint,
    int request_load_flags,
    RequestPriority request_priority,
    HttpNetworkSession* session,
    const ProxyInfo& proxy_info,
    const SSLConfig& ssl_config_for_origin,
    const SSLConfig& ssl_config_for_proxy,
    PrivacyMode privacy_mode,
    const NetLogWithSource& net_log,
    ClientSocketHandle* socket_handle,
    const OnHostResolutionCallback& resolution_callback,
    CompletionOnceCallback callback,
    const ClientSocketPool::ProxyAuthCallback& proxy_auth_callback) {
  DCHECK(socket_handle);

  // QUIC proxies are currently not supported through this method.
  DCHECK(!proxy_info.is_quic());

  return InitSocketPoolHelper(
      group_type, endpoint, request_load_flags, request_priority, session,
      proxy_info, ssl_config_for_origin, ssl_config_for_proxy,
      /*force_tunnel=*/true, privacy_mode, SocketTag(), net_log, 0,
      socket_handle, HttpNetworkSession::WEBSOCKET_SOCKET_POOL,
      resolution_callback, std::move(callback), proxy_auth_callback);
}

NET_EXPORT std::unique_ptr<ConnectJob> CreateConnectJobForRawConnect(
    const HostPortPair& host_port_pair,
    bool use_tls,
    const CommonConnectJobParams* common_connect_job_params,
    RequestPriority request_priority,
    const ProxyInfo& proxy_info,
    const SSLConfig& ssl_config_for_origin,
    const SSLConfig& ssl_config_for_proxy,
    const NetLogWithSource& net_log,
    ConnectJob::Delegate* connect_job_delegate) {
  // QUIC proxies are currently not supported through this method.
  DCHECK(!proxy_info.is_quic());

  ClientSocketPool::GroupId unused_connection_group;
  scoped_refptr<ClientSocketPool::SocketParams> socket_params =
      CreateSocketParamsAndGetGroupId(
          use_tls ? ClientSocketPoolManager::SSL_GROUP
                  : ClientSocketPoolManager::NORMAL_GROUP,
          host_port_pair, proxy_info, ssl_config_for_origin,
          ssl_config_for_proxy, true /* force_tunnel */,
          net::PRIVACY_MODE_DISABLED, OnHostResolutionCallback(),
          &unused_connection_group);
  return socket_params->create_connect_job_callback().Run(
      request_priority, SocketTag(), common_connect_job_params,
      connect_job_delegate);
}

int PreconnectSocketsForHttpRequest(
    ClientSocketPoolManager::SocketGroupType group_type,
    const HostPortPair& endpoint,
    int request_load_flags,
    RequestPriority request_priority,
    HttpNetworkSession* session,
    const ProxyInfo& proxy_info,
    const SSLConfig& ssl_config_for_origin,
    const SSLConfig& ssl_config_for_proxy,
    PrivacyMode privacy_mode,
    const NetLogWithSource& net_log,
    int num_preconnect_streams) {
  // QUIC proxies are currently not supported through this method.
  DCHECK(!proxy_info.is_quic());

  return InitSocketPoolHelper(
      group_type, endpoint, request_load_flags, request_priority, session,
      proxy_info, ssl_config_for_origin, ssl_config_for_proxy,
      /*force_tunnel=*/false, privacy_mode, SocketTag(), net_log,
      num_preconnect_streams, nullptr, HttpNetworkSession::NORMAL_SOCKET_POOL,
      OnHostResolutionCallback(), CompletionOnceCallback(),
      ClientSocketPool::ProxyAuthCallback());
}

}  // namespace net
