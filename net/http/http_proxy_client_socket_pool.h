// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_PROXY_CLIENT_SOCKET_POOL_H_
#define NET_HTTP_HTTP_PROXY_CLIENT_SOCKET_POOL_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_export.h"
#include "net/http/http_proxy_connect_job.h"
#include "net/socket/client_socket_pool.h"
#include "net/socket/client_socket_pool_base.h"

namespace net {

class NetLog;
class NetworkQualityEstimator;
class ProxyDelegate;
class TransportClientSocketPool;

class NET_EXPORT_PRIVATE HttpProxyClientSocketPool
    : public ClientSocketPool,
      public HigherLayeredPool {
 public:
  typedef HttpProxySocketParams SocketParams;

  HttpProxyClientSocketPool(int max_sockets,
                            int max_sockets_per_group,
                            TransportClientSocketPool* transport_pool,
                            TransportClientSocketPool* ssl_pool,
                            ProxyDelegate* proxy_delegate,
                            NetworkQualityEstimator* network_quality_estimator,
                            NetLog* net_log);

  ~HttpProxyClientSocketPool() override;

  // ClientSocketPool implementation.
  int RequestSocket(const std::string& group_name,
                    const void* connect_params,
                    RequestPriority priority,
                    const SocketTag& socket_tag,
                    RespectLimits respect_limits,
                    ClientSocketHandle* handle,
                    CompletionOnceCallback callback,
                    const NetLogWithSource& net_log) override;

  void RequestSockets(const std::string& group_name,
                      const void* params,
                      int num_sockets,
                      const NetLogWithSource& net_log) override;

  void SetPriority(const std::string& group_name,
                   ClientSocketHandle* handle,
                   RequestPriority priority) override;

  void CancelRequest(const std::string& group_name,
                     ClientSocketHandle* handle) override;

  void ReleaseSocket(const std::string& group_name,
                     std::unique_ptr<StreamSocket> socket,
                     int id) override;

  void FlushWithError(int error) override;

  void CloseIdleSockets() override;

  void CloseIdleSocketsInGroup(const std::string& group_name) override;

  int IdleSocketCount() const override;

  int IdleSocketCountInGroup(const std::string& group_name) const override;

  LoadState GetLoadState(const std::string& group_name,
                         const ClientSocketHandle* handle) const override;

  std::unique_ptr<base::DictionaryValue> GetInfoAsValue(
      const std::string& name,
      const std::string& type,
      bool include_nested_pools) const override;

  // LowerLayeredPool implementation.
  bool IsStalled() const override;

  void AddHigherLayeredPool(HigherLayeredPool* higher_pool) override;

  void RemoveHigherLayeredPool(HigherLayeredPool* higher_pool) override;

  // HigherLayeredPool implementation.
  bool CloseOneIdleConnection() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(HttpProxyClientSocketPoolTest,
                           ProxyPoolTimeoutWithConnectionProperty);

  typedef ClientSocketPoolBase<HttpProxySocketParams> PoolBase;

  class NET_EXPORT_PRIVATE HttpProxyConnectJobFactory
      : public PoolBase::ConnectJobFactory {
   public:
    HttpProxyConnectJobFactory(
        TransportClientSocketPool* transport_pool,
        TransportClientSocketPool* ssl_pool,
        ProxyDelegate* proxy_delegate,
        NetworkQualityEstimator* network_quality_estimator,
        NetLog* net_log);

    // ClientSocketPoolBase::ConnectJobFactory methods.
    std::unique_ptr<ConnectJob> NewConnectJob(
        const std::string& group_name,
        const PoolBase::Request& request,
        ConnectJob::Delegate* delegate) const override;

   private:
    FRIEND_TEST_ALL_PREFIXES(HttpProxyClientSocketPoolTest,
                             ProxyPoolTimeoutWithConnectionProperty);

    TransportClientSocketPool* const transport_pool_;
    TransportClientSocketPool* const ssl_pool_;
    ProxyDelegate* const proxy_delegate_;
    NetworkQualityEstimator* const network_quality_estimator_;

    NetLog* net_log_;

    DISALLOW_COPY_AND_ASSIGN(HttpProxyConnectJobFactory);
  };

  TransportClientSocketPool* const transport_pool_;
  TransportClientSocketPool* const ssl_pool_;
  PoolBase base_;

  DISALLOW_COPY_AND_ASSIGN(HttpProxyClientSocketPool);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_PROXY_CLIENT_SOCKET_POOL_H_
