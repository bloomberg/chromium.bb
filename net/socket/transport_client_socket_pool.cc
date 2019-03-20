// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/transport_client_socket_pool.h"

#include <utility>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "net/base/trace_constants.h"
#include "net/http/http_proxy_connect_job.h"
#include "net/log/net_log.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source_type.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/client_socket_pool_base.h"
#include "net/socket/connect_job.h"
#include "net/socket/socket_net_log_params.h"
#include "net/socket/socks_connect_job.h"
#include "net/socket/ssl_connect_job.h"
#include "net/socket/transport_connect_job.h"
#include "net/socket/websocket_transport_connect_job.h"

namespace net {

namespace {

std::unique_ptr<base::Value> NetLogGroupIdCallback(
    const ClientSocketPool::GroupId* group_id,
    NetLogCaptureMode /* capture_mode */) {
  std::unique_ptr<base::DictionaryValue> event_params(
      new base::DictionaryValue());
  event_params->SetString("group_id", group_id->ToString());
  return event_params;
}

// TODO(mmenke): Once the socket pool arguments are no longer needed, remove
// this method and use TransportConnectJob::CreateTransportConnectJob()
// directly.
std::unique_ptr<ConnectJob> CreateTransportConnectJob(
    scoped_refptr<TransportSocketParams> transport_socket_params,
    RequestPriority priority,
    const SocketTag& socket_tag,
    const CommonConnectJobParams* common_connect_job_params,
    ConnectJob::Delegate* delegate) {
  return TransportConnectJob::CreateTransportConnectJob(
      std::move(transport_socket_params), priority, socket_tag,
      common_connect_job_params, delegate, nullptr /* net_log */);
}

std::unique_ptr<ConnectJob> CreateSOCKSConnectJob(
    scoped_refptr<SOCKSSocketParams> socks_socket_params,
    RequestPriority priority,
    const SocketTag& socket_tag,
    const CommonConnectJobParams* common_connect_job_params,
    ConnectJob::Delegate* delegate) {
  return std::make_unique<SOCKSConnectJob>(
      priority, socket_tag, common_connect_job_params,
      std::move(socks_socket_params), delegate, nullptr /* net_log */);
}

std::unique_ptr<ConnectJob> CreateSSLConnectJob(
    scoped_refptr<SSLSocketParams> ssl_socket_params,
    RequestPriority priority,
    const SocketTag& socket_tag,
    const CommonConnectJobParams* common_connect_job_params,
    ConnectJob::Delegate* delegate) {
  return std::make_unique<SSLConnectJob>(
      priority, socket_tag, common_connect_job_params,
      std::move(ssl_socket_params), delegate, nullptr /* net_log */);
}

std::unique_ptr<ConnectJob> CreateHttpProxyConnectJob(
    scoped_refptr<HttpProxySocketParams> http_proxy_socket_params,
    RequestPriority priority,
    const SocketTag& socket_tag,
    const CommonConnectJobParams* common_connect_job_params,
    ConnectJob::Delegate* delegate) {
  return std::make_unique<HttpProxyConnectJob>(
      priority, socket_tag, common_connect_job_params,
      std::move(http_proxy_socket_params), delegate, nullptr /* net_log */);
}

}  // namespace

TransportClientSocketPool::SocketParams::SocketParams(
    const CreateConnectJobCallback& create_connect_job_callback)
    : create_connect_job_callback_(create_connect_job_callback) {}

scoped_refptr<TransportClientSocketPool::SocketParams>
TransportClientSocketPool::SocketParams::CreateFromTransportSocketParams(
    scoped_refptr<TransportSocketParams> transport_client_params) {
  CreateConnectJobCallback callback = base::BindRepeating(
      &CreateTransportConnectJob, std::move(transport_client_params));
  return base::MakeRefCounted<SocketParams>(callback);
}

scoped_refptr<TransportClientSocketPool::SocketParams>
TransportClientSocketPool::SocketParams::CreateFromSOCKSSocketParams(
    scoped_refptr<SOCKSSocketParams> socks_socket_params) {
  CreateConnectJobCallback callback = base::BindRepeating(
      &CreateSOCKSConnectJob, std::move(socks_socket_params));
  return base::MakeRefCounted<SocketParams>(callback);
}

scoped_refptr<TransportClientSocketPool::SocketParams>
TransportClientSocketPool::SocketParams::CreateFromSSLSocketParams(
    scoped_refptr<SSLSocketParams> ssl_socket_params) {
  CreateConnectJobCallback callback =
      base::BindRepeating(&CreateSSLConnectJob, std::move(ssl_socket_params));
  return base::MakeRefCounted<SocketParams>(callback);
}

scoped_refptr<TransportClientSocketPool::SocketParams>
TransportClientSocketPool::SocketParams::CreateFromHttpProxySocketParams(
    scoped_refptr<HttpProxySocketParams> http_proxy_socket_params) {
  CreateConnectJobCallback callback = base::BindRepeating(
      &CreateHttpProxyConnectJob, std::move(http_proxy_socket_params));
  return base::MakeRefCounted<SocketParams>(callback);
}

TransportClientSocketPool::SocketParams::~SocketParams() = default;

TransportClientSocketPool::TransportConnectJobFactory::
    TransportConnectJobFactory(
        const CommonConnectJobParams* common_connect_job_params)
    : common_connect_job_params_(common_connect_job_params) {}

TransportClientSocketPool::TransportConnectJobFactory::
    ~TransportConnectJobFactory() = default;

std::unique_ptr<ConnectJob>
TransportClientSocketPool::TransportConnectJobFactory::NewConnectJob(
    const PoolBase::Request& request,
    ConnectJob::Delegate* delegate) const {
  return request.params()->create_connect_job_callback().Run(
      request.priority(), request.socket_tag(), common_connect_job_params_,
      delegate);
}

TransportClientSocketPool::TransportClientSocketPool(
    int max_sockets,
    int max_sockets_per_group,
    base::TimeDelta unused_idle_socket_timeout,
    const CommonConnectJobParams* common_connect_job_params,
    SSLConfigService* ssl_config_service)
    : base_(max_sockets,
            max_sockets_per_group,
            unused_idle_socket_timeout,
            ClientSocketPool::used_idle_socket_timeout(),
            new TransportConnectJobFactory(common_connect_job_params)),
      client_socket_factory_(common_connect_job_params->client_socket_factory),
      ssl_config_service_(ssl_config_service) {
  base_.EnableConnectBackupJobs();
  if (ssl_config_service_)
    ssl_config_service_->AddObserver(this);
}

TransportClientSocketPool::~TransportClientSocketPool() {
  if (ssl_config_service_)
    ssl_config_service_->RemoveObserver(this);
}

int TransportClientSocketPool::RequestSocket(
    const GroupId& group_id,
    const void* params,
    RequestPriority priority,
    const SocketTag& socket_tag,
    RespectLimits respect_limits,
    ClientSocketHandle* handle,
    CompletionOnceCallback callback,
    const ProxyAuthCallback& proxy_auth_callback,
    const NetLogWithSource& net_log) {
  const scoped_refptr<SocketParams>* casted_params =
      static_cast<const scoped_refptr<SocketParams>*>(params);

  NetLogTcpClientSocketPoolRequestedSocket(net_log, group_id);

  return base_.RequestSocket(group_id, *casted_params, priority, socket_tag,
                             respect_limits, handle, std::move(callback),
                             proxy_auth_callback, net_log);
}

void TransportClientSocketPool::NetLogTcpClientSocketPoolRequestedSocket(
    const NetLogWithSource& net_log,
    const GroupId& group_id) {
  if (net_log.IsCapturing()) {
    // TODO(eroman): Split out the host and port parameters.
    net_log.AddEvent(NetLogEventType::TCP_CLIENT_SOCKET_POOL_REQUESTED_SOCKET,
                     base::BindRepeating(&NetLogGroupIdCallback,
                                         base::Unretained(&group_id)));
  }
}

void TransportClientSocketPool::RequestSockets(
    const GroupId& group_id,
    const void* params,
    int num_sockets,
    const NetLogWithSource& net_log) {
  const scoped_refptr<SocketParams>* casted_params =
      static_cast<const scoped_refptr<SocketParams>*>(params);

  if (net_log.IsCapturing()) {
    // TODO(eroman): Split out the host and port parameters.
    net_log.AddEvent(NetLogEventType::TCP_CLIENT_SOCKET_POOL_REQUESTED_SOCKETS,
                     base::BindRepeating(&NetLogGroupIdCallback,
                                         base::Unretained(&group_id)));
  }

  base_.RequestSockets(group_id, *casted_params, num_sockets, net_log);
}

void TransportClientSocketPool::SetPriority(const GroupId& group_id,
                                            ClientSocketHandle* handle,
                                            RequestPriority priority) {
  base_.SetPriority(group_id, handle, priority);
}

void TransportClientSocketPool::CancelRequest(const GroupId& group_id,
                                              ClientSocketHandle* handle) {
  base_.CancelRequest(group_id, handle);
}

void TransportClientSocketPool::ReleaseSocket(
    const GroupId& group_id,
    std::unique_ptr<StreamSocket> socket,
    int id) {
  base_.ReleaseSocket(group_id, std::move(socket), id);
}

void TransportClientSocketPool::FlushWithError(int error) {
  base_.FlushWithError(error);
}

void TransportClientSocketPool::CloseIdleSockets() {
  base_.CloseIdleSockets();
}

void TransportClientSocketPool::CloseIdleSocketsInGroup(
    const GroupId& group_id) {
  base_.CloseIdleSocketsInGroup(group_id);
}

int TransportClientSocketPool::IdleSocketCount() const {
  return base_.idle_socket_count();
}

size_t TransportClientSocketPool::IdleSocketCountInGroup(
    const GroupId& group_id) const {
  return base_.IdleSocketCountInGroup(group_id);
}

LoadState TransportClientSocketPool::GetLoadState(
    const GroupId& group_id,
    const ClientSocketHandle* handle) const {
  return base_.GetLoadState(group_id, handle);
}

std::unique_ptr<base::DictionaryValue>
TransportClientSocketPool::GetInfoAsValue(const std::string& name,
                                          const std::string& type) const {
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

void TransportClientSocketPool::DumpMemoryStats(
    base::trace_event::ProcessMemoryDump* pmd,
    const std::string& parent_dump_absolute_name) const {
  base_.DumpMemoryStats(pmd, parent_dump_absolute_name);
}

void TransportClientSocketPool::OnSSLConfigChanged() {
  // When the user changes the SSL config, flush all idle sockets so they won't
  // get re-used.
  FlushWithError(ERR_NETWORK_CHANGED);
}

}  // namespace net
