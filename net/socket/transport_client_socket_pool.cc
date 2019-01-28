// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/transport_client_socket_pool.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "net/base/trace_constants.h"
#include "net/log/net_log.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source_type.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/client_socket_pool_base.h"
#include "net/socket/socket_net_log_params.h"
#include "net/socket/socks_connect_job.h"
#include "net/socket/transport_connect_job.h"
#include "net/socket/websocket_transport_connect_job.h"

namespace net {

namespace {

std::unique_ptr<ConnectJob> CreateSOCKSConnectJob(
    scoped_refptr<SOCKSSocketParams> socks_socket_params,
    RequestPriority priority,
    const CommonConnectJobParams& common_connect_job_params,
    ConnectJob::Delegate* delegate) {
  return std::make_unique<SOCKSConnectJob>(priority, common_connect_job_params,
                                           socks_socket_params, delegate);
}

}  // namespace

TransportClientSocketPool::SocketParams::SocketParams(
    const CreateConnectJobCallback& create_connect_job_callback)
    : create_connect_job_callback_(create_connect_job_callback) {}

scoped_refptr<TransportClientSocketPool::SocketParams>
TransportClientSocketPool::SocketParams::CreateFromTransportSocketParams(
    scoped_refptr<TransportSocketParams> transport_client_params) {
  CreateConnectJobCallback callback =
      base::BindRepeating(&TransportConnectJob::CreateTransportConnectJob,
                          std::move(transport_client_params));
  return base::MakeRefCounted<SocketParams>(callback);
}

scoped_refptr<TransportClientSocketPool::SocketParams>
TransportClientSocketPool::SocketParams::CreateFromSOCKSSocketParams(
    scoped_refptr<SOCKSSocketParams> socks_socket_params) {
  CreateConnectJobCallback callback = base::BindRepeating(
      &CreateSOCKSConnectJob, std::move(socks_socket_params));
  return base::MakeRefCounted<SocketParams>(callback);
}

TransportClientSocketPool::SocketParams::~SocketParams() = default;

std::unique_ptr<ConnectJob>
TransportClientSocketPool::TransportConnectJobFactory::NewConnectJob(
    const std::string& group_name,
    const PoolBase::Request& request,
    ConnectJob::Delegate* delegate) const {
  return request.params()->create_connect_job_callback().Run(
      request.priority(),
      CommonConnectJobParams(
          group_name, request.socket_tag(),
          request.respect_limits() == ClientSocketPool::RespectLimits::ENABLED,
          client_socket_factory_, host_resolver_, ssl_client_socket_context_,
          socket_performance_watcher_factory_, network_quality_estimator_,
          net_log_, nullptr /* websocket_endpoint_lock_manager */),
      delegate);
}

TransportClientSocketPool::TransportClientSocketPool(
    int max_sockets,
    int max_sockets_per_group,
    ClientSocketFactory* client_socket_factory,
    HostResolver* host_resolver,
    CertVerifier* cert_verifier,
    ChannelIDService* channel_id_service,
    TransportSecurityState* transport_security_state,
    CTVerifier* cert_transparency_verifier,
    CTPolicyEnforcer* ct_policy_enforcer,
    const std::string& ssl_session_cache_shard,
    SSLConfigService* ssl_config_service,
    SocketPerformanceWatcherFactory* socket_performance_watcher_factory,
    NetworkQualityEstimator* network_quality_estimator,
    NetLog* net_log)
    : base_(nullptr,
            max_sockets,
            max_sockets_per_group,
            ClientSocketPool::unused_idle_socket_timeout(),
            ClientSocketPool::used_idle_socket_timeout(),
            new TransportConnectJobFactory(
                client_socket_factory,
                host_resolver,
                SSLClientSocketContext(cert_verifier,
                                       channel_id_service,
                                       transport_security_state,
                                       cert_transparency_verifier,
                                       ct_policy_enforcer,
                                       ssl_session_cache_shard),
                socket_performance_watcher_factory,
                network_quality_estimator,
                net_log)),
      client_socket_factory_(client_socket_factory),
      ssl_config_service_(ssl_config_service) {
  base_.EnableConnectBackupJobs();
  if (ssl_config_service_)
    ssl_config_service_->AddObserver(this);
}

TransportClientSocketPool::~TransportClientSocketPool() {
  if (ssl_config_service_)
    ssl_config_service_->RemoveObserver(this);
}

int TransportClientSocketPool::RequestSocket(const std::string& group_name,
                                             const void* params,
                                             RequestPriority priority,
                                             const SocketTag& socket_tag,
                                             RespectLimits respect_limits,
                                             ClientSocketHandle* handle,
                                             CompletionOnceCallback callback,
                                             const NetLogWithSource& net_log) {
  const scoped_refptr<SocketParams>* casted_params =
      static_cast<const scoped_refptr<SocketParams>*>(params);

  NetLogTcpClientSocketPoolRequestedSocket(net_log, group_name);

  return base_.RequestSocket(group_name, *casted_params, priority, socket_tag,
                             respect_limits, handle, std::move(callback),
                             net_log);
}

void TransportClientSocketPool::NetLogTcpClientSocketPoolRequestedSocket(
    const NetLogWithSource& net_log,
    const std::string& group_name) {
  if (net_log.IsCapturing()) {
    // TODO(eroman): Split out the host and port parameters.
    net_log.AddEvent(NetLogEventType::TCP_CLIENT_SOCKET_POOL_REQUESTED_SOCKET,
                     NetLog::StringCallback("group", &group_name));
  }
}

void TransportClientSocketPool::RequestSockets(
    const std::string& group_name,
    const void* params,
    int num_sockets,
    const NetLogWithSource& net_log) {
  const scoped_refptr<SocketParams>* casted_params =
      static_cast<const scoped_refptr<SocketParams>*>(params);

  if (net_log.IsCapturing()) {
    // TODO(eroman): Split out the host and port parameters.
    net_log.AddEvent(NetLogEventType::TCP_CLIENT_SOCKET_POOL_REQUESTED_SOCKETS,
                     NetLog::StringCallback("group_name", &group_name));
  }

  base_.RequestSockets(group_name, *casted_params, num_sockets, net_log);
}

void TransportClientSocketPool::SetPriority(const std::string& group_name,
                                            ClientSocketHandle* handle,
                                            RequestPriority priority) {
  base_.SetPriority(group_name, handle, priority);
}

void TransportClientSocketPool::CancelRequest(
    const std::string& group_name,
    ClientSocketHandle* handle) {
  base_.CancelRequest(group_name, handle);
}

void TransportClientSocketPool::ReleaseSocket(
    const std::string& group_name,
    std::unique_ptr<StreamSocket> socket,
    int id) {
  base_.ReleaseSocket(group_name, std::move(socket), id);
}

void TransportClientSocketPool::FlushWithError(int error) {
  base_.FlushWithError(error);
}

void TransportClientSocketPool::CloseIdleSockets() {
  base_.CloseIdleSockets();
}

void TransportClientSocketPool::CloseIdleSocketsInGroup(
    const std::string& group_name) {
  base_.CloseIdleSocketsInGroup(group_name);
}

int TransportClientSocketPool::IdleSocketCount() const {
  return base_.idle_socket_count();
}

int TransportClientSocketPool::IdleSocketCountInGroup(
    const std::string& group_name) const {
  return base_.IdleSocketCountInGroup(group_name);
}

LoadState TransportClientSocketPool::GetLoadState(
    const std::string& group_name, const ClientSocketHandle* handle) const {
  return base_.GetLoadState(group_name, handle);
}

std::unique_ptr<base::DictionaryValue>
TransportClientSocketPool::GetInfoAsValue(const std::string& name,
                                          const std::string& type,
                                          bool include_nested_pools) const {
  return base_.GetInfoAsValue(name, type);
}

bool TransportClientSocketPool::IsStalled() const {
  return base_.IsStalled();
}

void TransportClientSocketPool::AddHigherLayeredPool(
    HigherLayeredPool* higher_pool) {
  base_.AddHigherLayeredPool(higher_pool);
}

void TransportClientSocketPool::RemoveHigherLayeredPool(
    HigherLayeredPool* higher_pool) {
  base_.RemoveHigherLayeredPool(higher_pool);
}

void TransportClientSocketPool::OnSSLConfigChanged() {
  // When the user changes the SSL config, flush all idle sockets so they won't
  // get re-used.
  FlushWithError(ERR_NETWORK_CHANGED);
}

}  // namespace net
