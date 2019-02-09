// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_proxy_client_socket_pool.h"

#include "net/socket/client_socket_pool_base.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/ssl_connect_job.h"
#include "net/socket/transport_client_socket_pool.h"

namespace net {

HttpProxyClientSocketPool::HttpProxyConnectJobFactory::
    HttpProxyConnectJobFactory(
        TransportClientSocketPool* transport_pool,
        TransportClientSocketPool* ssl_pool,
        ProxyDelegate* proxy_delegate,
        NetworkQualityEstimator* network_quality_estimator,
        NetLog* net_log)
    : transport_pool_(transport_pool),
      ssl_pool_(ssl_pool),
      proxy_delegate_(proxy_delegate),
      network_quality_estimator_(network_quality_estimator),
      net_log_(net_log) {}

std::unique_ptr<ConnectJob>
HttpProxyClientSocketPool::HttpProxyConnectJobFactory::NewConnectJob(
    const std::string& group_name,
    const PoolBase::Request& request,
    ConnectJob::Delegate* delegate) const {
  return std::make_unique<HttpProxyConnectJob>(
      group_name, request.priority(), request.socket_tag(),
      request.respect_limits() == ClientSocketPool::RespectLimits::ENABLED,
      request.params(), proxy_delegate_, transport_pool_, ssl_pool_,
      network_quality_estimator_, delegate, net_log_);
}

HttpProxyClientSocketPool::HttpProxyClientSocketPool(
    int max_sockets,
    int max_sockets_per_group,
    TransportClientSocketPool* transport_pool,
    TransportClientSocketPool* ssl_pool,
    ProxyDelegate* proxy_delegate,
    NetworkQualityEstimator* network_quality_estimator,
    NetLog* net_log)
    : transport_pool_(transport_pool),
      ssl_pool_(ssl_pool),
      base_(this,
            max_sockets,
            max_sockets_per_group,
            ClientSocketPool::unused_idle_socket_timeout(),
            ClientSocketPool::used_idle_socket_timeout(),
            new HttpProxyConnectJobFactory(transport_pool,
                                           ssl_pool,
                                           proxy_delegate,
                                           network_quality_estimator,
                                           net_log)) {
  // We should always have a |transport_pool_| except in unit tests.
  if (transport_pool_)
    base_.AddLowerLayeredPool(transport_pool_);
  if (ssl_pool_)
    base_.AddLowerLayeredPool(ssl_pool_);
}

HttpProxyClientSocketPool::~HttpProxyClientSocketPool() = default;

int HttpProxyClientSocketPool::RequestSocket(const std::string& group_name,
                                             const void* socket_params,
                                             RequestPriority priority,
                                             const SocketTag& socket_tag,
                                             RespectLimits respect_limits,
                                             ClientSocketHandle* handle,
                                             CompletionOnceCallback callback,
                                             const NetLogWithSource& net_log) {
  const scoped_refptr<HttpProxySocketParams>* casted_socket_params =
      static_cast<const scoped_refptr<HttpProxySocketParams>*>(socket_params);

  return base_.RequestSocket(group_name, *casted_socket_params, priority,
                             socket_tag, respect_limits, handle,
                             std::move(callback), net_log);
}

void HttpProxyClientSocketPool::RequestSockets(
    const std::string& group_name,
    const void* params,
    int num_sockets,
    const NetLogWithSource& net_log) {
  const scoped_refptr<HttpProxySocketParams>* casted_params =
      static_cast<const scoped_refptr<HttpProxySocketParams>*>(params);

  base_.RequestSockets(group_name, *casted_params, num_sockets, net_log);
}

void HttpProxyClientSocketPool::CancelRequest(
    const std::string& group_name,
    ClientSocketHandle* handle) {
  base_.CancelRequest(group_name, handle);
}

void HttpProxyClientSocketPool::SetPriority(const std::string& group_name,
                                            ClientSocketHandle* handle,
                                            RequestPriority priority) {
  base_.SetPriority(group_name, handle, priority);
}

void HttpProxyClientSocketPool::ReleaseSocket(
    const std::string& group_name,
    std::unique_ptr<StreamSocket> socket,
    int id) {
  base_.ReleaseSocket(group_name, std::move(socket), id);
}

void HttpProxyClientSocketPool::FlushWithError(int error) {
  base_.FlushWithError(error);
}

void HttpProxyClientSocketPool::CloseIdleSockets() {
  base_.CloseIdleSockets();
}

void HttpProxyClientSocketPool::CloseIdleSocketsInGroup(
    const std::string& group_name) {
  base_.CloseIdleSocketsInGroup(group_name);
}

int HttpProxyClientSocketPool::IdleSocketCount() const {
  return base_.idle_socket_count();
}

int HttpProxyClientSocketPool::IdleSocketCountInGroup(
    const std::string& group_name) const {
  return base_.IdleSocketCountInGroup(group_name);
}

LoadState HttpProxyClientSocketPool::GetLoadState(
    const std::string& group_name, const ClientSocketHandle* handle) const {
  return base_.GetLoadState(group_name, handle);
}

std::unique_ptr<base::DictionaryValue>
HttpProxyClientSocketPool::GetInfoAsValue(const std::string& name,
                                          const std::string& type,
                                          bool include_nested_pools) const {
  std::unique_ptr<base::DictionaryValue> dict(base_.GetInfoAsValue(name, type));
  if (include_nested_pools) {
    auto list = std::make_unique<base::ListValue>();
    if (transport_pool_) {
      list->Append(transport_pool_->GetInfoAsValue("transport_socket_pool",
                                                   "transport_socket_pool",
                                                   true));
    }
    if (ssl_pool_) {
      list->Append(ssl_pool_->GetInfoAsValue("ssl_socket_pool",
                                             "ssl_socket_pool",
                                             true));
    }
    dict->Set("nested_pools", std::move(list));
  }
  return dict;
}

bool HttpProxyClientSocketPool::IsStalled() const {
  return base_.IsStalled();
}

void HttpProxyClientSocketPool::AddHigherLayeredPool(
    HigherLayeredPool* higher_pool) {
  base_.AddHigherLayeredPool(higher_pool);
}

void HttpProxyClientSocketPool::RemoveHigherLayeredPool(
    HigherLayeredPool* higher_pool) {
  base_.RemoveHigherLayeredPool(higher_pool);
}

bool HttpProxyClientSocketPool::CloseOneIdleConnection() {
  if (base_.CloseOneIdleSocket())
    return true;
  return base_.CloseOneIdleConnectionInHigherLayeredPool();
}

}  // namespace net
