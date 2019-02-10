// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_MOCK_CLIENT_SOCKET_POOL_MANAGER_H_
#define NET_SOCKET_MOCK_CLIENT_SOCKET_POOL_MANAGER_H_

#include <map>
#include <memory>
#include <string>

#include "base/macros.h"
#include "net/base/proxy_server.h"
#include "net/socket/client_socket_pool_manager.h"
#include "net/socket/client_socket_pool_manager_impl.h"

namespace net {

class MockClientSocketPoolManager : public ClientSocketPoolManager {
 public:
  MockClientSocketPoolManager();
  ~MockClientSocketPoolManager() override;

  // Sets "override" socket pools that get used instead.
  void SetTransportSocketPool(TransportClientSocketPool* pool);
  // Currently only works for SOCKS proxies.
  void SetSocketPoolForProxy(const ProxyServer& proxy_server,
                             std::unique_ptr<TransportClientSocketPool> pool);
  void SetSocketPoolForHTTPProxy(
      const ProxyServer& http_proxy,
      std::unique_ptr<TransportClientSocketPool> pool);
  void SetSocketPoolForSSLWithProxy(
      const ProxyServer& proxy_server,
      std::unique_ptr<TransportClientSocketPool> pool);

  // ClientSocketPoolManager methods:
  void FlushSocketPoolsWithError(int error) override;
  void CloseIdleSockets() override;
  TransportClientSocketPool* GetTransportSocketPool() override;
  TransportClientSocketPool* GetSocketPoolForSOCKSProxy(
      const ProxyServer& socks_proxy) override;
  TransportClientSocketPool* GetSocketPoolForHTTPLikeProxy(
      const ProxyServer& http_proxy) override;
  TransportClientSocketPool* GetSocketPoolForSSLWithProxy(
      const ProxyServer& proxy_server) override;
  std::unique_ptr<base::Value> SocketPoolInfoToValue() const override;
  void DumpMemoryStats(
      base::trace_event::ProcessMemoryDump* pmd,
      const std::string& parent_dump_absolute_name) const override;

 private:
  using TransportClientSocketPoolMap =
      std::map<ProxyServer, std::unique_ptr<TransportClientSocketPool>>;

  std::unique_ptr<TransportClientSocketPool> transport_socket_pool_;
  TransportClientSocketPoolMap proxy_socket_pools_;
  TransportClientSocketPoolMap http_proxy_socket_pools_;
  TransportClientSocketPoolMap ssl_socket_pools_for_proxies_;

  DISALLOW_COPY_AND_ASSIGN(MockClientSocketPoolManager);
};

}  // namespace net

#endif  // NET_SOCKET_MOCK_CLIENT_SOCKET_POOL_MANAGER_H_
