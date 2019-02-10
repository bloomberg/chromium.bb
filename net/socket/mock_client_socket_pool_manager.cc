// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/mock_client_socket_pool_manager.h"

#include "base/values.h"
#include "net/socket/transport_client_socket_pool.h"

namespace net {

MockClientSocketPoolManager::MockClientSocketPoolManager() = default;
MockClientSocketPoolManager::~MockClientSocketPoolManager() = default;

void MockClientSocketPoolManager::SetTransportSocketPool(
    TransportClientSocketPool* pool) {
  transport_socket_pool_.reset(pool);
}

void MockClientSocketPoolManager::SetSocketPoolForProxy(
    const ProxyServer& proxy_server,
    std::unique_ptr<TransportClientSocketPool> pool) {
  DCHECK(proxy_server.is_socks());
  proxy_socket_pools_[proxy_server] = std::move(pool);
}

void MockClientSocketPoolManager::SetSocketPoolForHTTPProxy(
    const ProxyServer& http_proxy,
    std::unique_ptr<TransportClientSocketPool> pool) {
  http_proxy_socket_pools_[http_proxy] = std::move(pool);
}

void MockClientSocketPoolManager::SetSocketPoolForSSLWithProxy(
    const ProxyServer& proxy_server,
    std::unique_ptr<TransportClientSocketPool> pool) {
  ssl_socket_pools_for_proxies_[proxy_server] = std::move(pool);
}

void MockClientSocketPoolManager::FlushSocketPoolsWithError(int error) {
  NOTIMPLEMENTED();
}

void MockClientSocketPoolManager::CloseIdleSockets() {
  NOTIMPLEMENTED();
}

TransportClientSocketPool*
MockClientSocketPoolManager::GetTransportSocketPool() {
  return transport_socket_pool_.get();
}

TransportClientSocketPool*
MockClientSocketPoolManager::GetSocketPoolForSOCKSProxy(
    const ProxyServer& proxy_server) {
  DCHECK(proxy_server.is_socks());
  TransportClientSocketPoolMap::const_iterator it =
      proxy_socket_pools_.find(proxy_server);
  if (it != proxy_socket_pools_.end())
    return it->second.get();
  return nullptr;
}

TransportClientSocketPool*
MockClientSocketPoolManager::GetSocketPoolForHTTPLikeProxy(
    const ProxyServer& http_proxy) {
  TransportClientSocketPoolMap::const_iterator it =
      http_proxy_socket_pools_.find(http_proxy);
  if (it != http_proxy_socket_pools_.end())
    return it->second.get();
  return nullptr;
}

TransportClientSocketPool*
MockClientSocketPoolManager::GetSocketPoolForSSLWithProxy(
    const ProxyServer& proxy_server) {
  TransportClientSocketPoolMap::const_iterator it =
      ssl_socket_pools_for_proxies_.find(proxy_server);
  if (it != ssl_socket_pools_for_proxies_.end())
    return it->second.get();
  return nullptr;
}

std::unique_ptr<base::Value>
MockClientSocketPoolManager::SocketPoolInfoToValue() const {
  NOTIMPLEMENTED();
  return std::unique_ptr<base::Value>(nullptr);
}

void MockClientSocketPoolManager::DumpMemoryStats(
    base::trace_event::ProcessMemoryDump* pmd,
    const std::string& parent_dump_absolute_name) const {}

}  // namespace net
