// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/client_socket_pool_manager_impl.h"

#include <algorithm>
#include <utility>

#include "base/logging.h"
#include "base/values.h"
#include "net/base/proxy_server.h"
#include "net/http/http_network_session.h"
#include "net/socket/socks_connect_job.h"
#include "net/socket/ssl_connect_job.h"
#include "net/socket/transport_client_socket_pool.h"
#include "net/socket/transport_connect_job.h"
#include "net/socket/websocket_transport_client_socket_pool.h"
#include "net/ssl/ssl_config_service.h"

namespace net {

class SocketPerformanceWatcherFactory;

namespace {

// Appends information about all |socket_pools| to the end of |list|.
template <class MapType>
void AddSocketPoolsToList(base::ListValue* list,
                          const MapType& socket_pools,
                          const std::string& type,
                          bool include_nested_pools) {
  for (auto it = socket_pools.begin(); it != socket_pools.end(); it++) {
    list->Append(it->second->GetInfoAsValue(it->first.ToURI(), type,
                                            include_nested_pools));
  }
}

}  // namespace

ClientSocketPoolManagerImpl::ClientSocketPoolManagerImpl(
    NetLog* net_log,
    ClientSocketFactory* socket_factory,
    SocketPerformanceWatcherFactory* socket_performance_watcher_factory,
    NetworkQualityEstimator* network_quality_estimator,
    HostResolver* host_resolver,
    CertVerifier* cert_verifier,
    ChannelIDService* channel_id_service,
    TransportSecurityState* transport_security_state,
    CTVerifier* cert_transparency_verifier,
    CTPolicyEnforcer* ct_policy_enforcer,
    SSLClientSessionCache* ssl_client_session_cache,
    SSLClientSessionCache* ssl_client_session_cache_privacy_mode,
    SSLConfigService* ssl_config_service,
    WebSocketEndpointLockManager* websocket_endpoint_lock_manager,
    ProxyDelegate* proxy_delegate,
    HttpNetworkSession::SocketPoolType pool_type)
    : net_log_(net_log),
      socket_factory_(socket_factory),
      socket_performance_watcher_factory_(socket_performance_watcher_factory),
      network_quality_estimator_(network_quality_estimator),
      host_resolver_(host_resolver),
      cert_verifier_(cert_verifier),
      channel_id_service_(channel_id_service),
      transport_security_state_(transport_security_state),
      cert_transparency_verifier_(cert_transparency_verifier),
      ct_policy_enforcer_(ct_policy_enforcer),
      ssl_client_session_cache_(ssl_client_session_cache),
      ssl_client_session_cache_privacy_mode_(
          ssl_client_session_cache_privacy_mode),
      ssl_config_service_(ssl_config_service),
      proxy_delegate_(proxy_delegate),
      pool_type_(pool_type),
      transport_socket_pool_(
          pool_type == HttpNetworkSession::WEBSOCKET_SOCKET_POOL
              ? std::make_unique<WebSocketTransportClientSocketPool>(
                    max_sockets_per_pool(pool_type),
                    max_sockets_per_group(pool_type),
                    socket_factory_,
                    host_resolver,
                    proxy_delegate,
                    cert_verifier,
                    channel_id_service,
                    transport_security_state,
                    cert_transparency_verifier,
                    ct_policy_enforcer,
                    ssl_client_session_cache,
                    ssl_client_session_cache_privacy_mode,
                    ssl_config_service,
                    network_quality_estimator,
                    websocket_endpoint_lock_manager,
                    net_log)
              : CreateTransportSocketPool(
                    ProxyServer::Direct(),
                    true /* use_socket_performance_watcher_factory */)) {
  CertDatabase::GetInstance()->AddObserver(this);
}

ClientSocketPoolManagerImpl::~ClientSocketPoolManagerImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CertDatabase::GetInstance()->RemoveObserver(this);
}

void ClientSocketPoolManagerImpl::FlushSocketPoolsWithError(int error) {
  // Flush the highest level pools first, since higher level pools may release
  // stuff to the lower level pools.

  for (const auto& it : ssl_socket_pools_for_proxies_) {
    it.second->FlushWithError(error);
  }

  for (const auto& it : http_proxy_socket_pools_) {
    it.second->FlushWithError(error);
  }

  for (const auto& it : proxy_socket_pools_) {
    it.second->FlushWithError(error);
  }

  transport_socket_pool_->FlushWithError(error);
}

void ClientSocketPoolManagerImpl::CloseIdleSockets() {
  // Close sockets in the highest level pools first, since higher level pools'
  // sockets may release stuff to the lower level pools.
  for (const auto& it : ssl_socket_pools_for_proxies_) {
    it.second->CloseIdleSockets();
  }

  for (const auto& it : http_proxy_socket_pools_) {
    it.second->CloseIdleSockets();
  }

  for (const auto& it : proxy_socket_pools_) {
    it.second->CloseIdleSockets();
  }

  transport_socket_pool_->CloseIdleSockets();
}

TransportClientSocketPool*
ClientSocketPoolManagerImpl::GetTransportSocketPool() {
  return transport_socket_pool_.get();
}

TransportClientSocketPool*
ClientSocketPoolManagerImpl::GetSocketPoolForSOCKSProxy(
    const ProxyServer& proxy_server) {
  DCHECK(proxy_server.is_socks());

  TransportSocketPoolMap::const_iterator it =
      proxy_socket_pools_.find(proxy_server);
  if (it != proxy_socket_pools_.end())
    return it->second.get();

  std::pair<TransportSocketPoolMap::iterator, bool> ret =
      proxy_socket_pools_.insert(std::make_pair(
          proxy_server,
          CreateTransportSocketPool(
              proxy_server,
              false /* use_socket_performance_watcher_factory */)));

  return ret.first->second.get();
}

TransportClientSocketPool*
ClientSocketPoolManagerImpl::GetSocketPoolForHTTPLikeProxy(
    const ProxyServer& http_proxy) {
  DCHECK(http_proxy.is_http_like());

  TransportSocketPoolMap::const_iterator it =
      http_proxy_socket_pools_.find(http_proxy);
  if (it != http_proxy_socket_pools_.end())
    return it->second.get();

  std::pair<TransportSocketPoolMap::iterator, bool> ret =
      http_proxy_socket_pools_.insert(std::make_pair(
          http_proxy,
          CreateTransportSocketPool(
              http_proxy, true /* use_socket_performance_watcher_factory */)));

  return ret.first->second.get();
}

TransportClientSocketPool*
ClientSocketPoolManagerImpl::GetSocketPoolForSSLWithProxy(
    const ProxyServer& proxy_server) {
  DCHECK(!proxy_server.is_socks());
  DCHECK(!proxy_server.is_direct());

  TransportSocketPoolMap::const_iterator it =
      ssl_socket_pools_for_proxies_.find(proxy_server);
  if (it != ssl_socket_pools_for_proxies_.end())
    return it->second.get();

  int sockets_per_proxy_server = max_sockets_per_proxy_server(pool_type_);
  int sockets_per_group = std::min(sockets_per_proxy_server,
                                   max_sockets_per_group(pool_type_));

  std::pair<TransportSocketPoolMap::iterator, bool> ret =
      ssl_socket_pools_for_proxies_.insert(std::make_pair(
          proxy_server,
          std::make_unique<TransportClientSocketPool>(
              sockets_per_proxy_server, sockets_per_group, socket_factory_,
              host_resolver_, proxy_delegate_, cert_verifier_,
              channel_id_service_, transport_security_state_,
              cert_transparency_verifier_, ct_policy_enforcer_,
              ssl_client_session_cache_, ssl_client_session_cache_privacy_mode_,
              ssl_config_service_, socket_performance_watcher_factory_,
              network_quality_estimator_, net_log_,
              proxy_server.is_http_like()
                  ? GetSocketPoolForHTTPLikeProxy(proxy_server)
                  : nullptr)));

  return ret.first->second.get();
}

std::unique_ptr<base::Value>
ClientSocketPoolManagerImpl::SocketPoolInfoToValue() const {
  std::unique_ptr<base::ListValue> list(new base::ListValue());
  list->Append(transport_socket_pool_->GetInfoAsValue("transport_socket_pool",
                                                "transport_socket_pool",
                                                false));
  AddSocketPoolsToList(list.get(), http_proxy_socket_pools_,
                       "http_proxy_socket_pool", true);
  AddSocketPoolsToList(list.get(), proxy_socket_pools_, "proxy_socket_pools",
                       true);

  // Third parameter is false because |ssl_socket_pools_for_proxies_| use
  // socket pools in |http_proxy_socket_pools_| and |socks_socket_pools_|.
  AddSocketPoolsToList(list.get(), ssl_socket_pools_for_proxies_,
                       "ssl_socket_pool_for_proxies", false);
  return std::move(list);
}

void ClientSocketPoolManagerImpl::OnCertDBChanged() {
  FlushSocketPoolsWithError(ERR_NETWORK_CHANGED);
}

void ClientSocketPoolManagerImpl::DumpMemoryStats(
    base::trace_event::ProcessMemoryDump* pmd,
    const std::string& parent_dump_absolute_name) const {
  return transport_socket_pool_->DumpMemoryStats(pmd,
                                                 parent_dump_absolute_name);
}

std::unique_ptr<TransportClientSocketPool>
ClientSocketPoolManagerImpl::CreateTransportSocketPool(
    const ProxyServer& proxy_server,
    bool use_socket_performance_watcher_factory) {
  int sockets_per_proxy_server;
  int sockets_per_group;
  if (proxy_server.is_direct()) {
    sockets_per_proxy_server = max_sockets_per_pool(pool_type_);
    sockets_per_group = max_sockets_per_group(pool_type_);
  } else {
    sockets_per_proxy_server = max_sockets_per_proxy_server(pool_type_);
    sockets_per_group =
        std::min(sockets_per_proxy_server, max_sockets_per_group(pool_type_));
  }
  return std::make_unique<TransportClientSocketPool>(
      sockets_per_proxy_server, sockets_per_group, socket_factory_,
      host_resolver_, proxy_delegate_, cert_verifier_, channel_id_service_,
      transport_security_state_, cert_transparency_verifier_,
      ct_policy_enforcer_, ssl_client_session_cache_,
      ssl_client_session_cache_privacy_mode_, ssl_config_service_,
      use_socket_performance_watcher_factory
          ? socket_performance_watcher_factory_
          : nullptr,
      network_quality_estimator_, net_log_);
}

}  // namespace net
