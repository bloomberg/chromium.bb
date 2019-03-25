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

}  // namespace

TransportClientSocketPool::TransportConnectJobFactory::
    TransportConnectJobFactory(
        const CommonConnectJobParams* common_connect_job_params)
    : common_connect_job_params_(common_connect_job_params) {}

TransportClientSocketPool::TransportConnectJobFactory::
    ~TransportConnectJobFactory() = default;

std::unique_ptr<ConnectJob>
TransportClientSocketPool::TransportConnectJobFactory::NewConnectJob(
    RequestPriority request_priority,
    SocketTag socket_tag,
    scoped_refptr<SocketParams> socket_params,
    ConnectJob::Delegate* delegate) const {
  return socket_params->create_connect_job_callback().Run(
      request_priority, socket_tag, common_connect_job_params_, delegate);
}

TransportClientSocketPool::TransportClientSocketPool(
    int max_sockets,
    int max_sockets_per_group,
    base::TimeDelta unused_idle_socket_timeout,
    const CommonConnectJobParams* common_connect_job_params,
    SSLConfigService* ssl_config_service)
    : TransportClientSocketPool(max_sockets,
                                max_sockets_per_group,
                                unused_idle_socket_timeout,
                                ClientSocketPool::used_idle_socket_timeout(),
                                std::make_unique<TransportConnectJobFactory>(
                                    common_connect_job_params),
                                ssl_config_service,
                                true /* connect_backup_jobs_enabled */) {}

TransportClientSocketPool::~TransportClientSocketPool() {
  if (ssl_config_service_)
    ssl_config_service_->RemoveObserver(this);
}

std::unique_ptr<TransportClientSocketPool>
TransportClientSocketPool::CreateForTesting(
    int max_sockets,
    int max_sockets_per_group,
    base::TimeDelta unused_idle_socket_timeout,
    base::TimeDelta used_idle_socket_timeout,
    std::unique_ptr<ConnectJobFactory> connect_job_factory,
    SSLConfigService* ssl_config_service,
    bool connect_backup_jobs_enabled) {
  return base::WrapUnique<TransportClientSocketPool>(
      new TransportClientSocketPool(
          max_sockets, max_sockets_per_group, unused_idle_socket_timeout,
          used_idle_socket_timeout, std::move(connect_job_factory),
          ssl_config_service, connect_backup_jobs_enabled));
}

int TransportClientSocketPool::RequestSocket(
    const GroupId& group_id,
    scoped_refptr<SocketParams> socket_params,
    RequestPriority priority,
    const SocketTag& socket_tag,
    RespectLimits respect_limits,
    ClientSocketHandle* handle,
    CompletionOnceCallback callback,
    const ProxyAuthCallback& proxy_auth_callback,
    const NetLogWithSource& net_log) {
  NetLogTcpClientSocketPoolRequestedSocket(net_log, group_id);

  return ClientSocketPoolBaseHelper::RequestSocket(
      group_id, socket_params, priority, socket_tag, respect_limits, handle,
      std::move(callback), proxy_auth_callback, net_log);
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
    scoped_refptr<SocketParams> socket_params,
    int num_sockets,
    const NetLogWithSource& net_log) {
  if (net_log.IsCapturing()) {
    // TODO(eroman): Split out the host and port parameters.
    net_log.AddEvent(NetLogEventType::TCP_CLIENT_SOCKET_POOL_REQUESTED_SOCKETS,
                     base::BindRepeating(&NetLogGroupIdCallback,
                                         base::Unretained(&group_id)));
  }

  ClientSocketPoolBaseHelper::RequestSockets(group_id, socket_params,
                                             num_sockets, net_log);
}

TransportClientSocketPool::TransportClientSocketPool(
    int max_sockets,
    int max_sockets_per_group,
    base::TimeDelta unused_idle_socket_timeout,
    base::TimeDelta used_idle_socket_timeout,
    std::unique_ptr<ConnectJobFactory> connect_job_factory,
    SSLConfigService* ssl_config_service,
    bool connect_backup_jobs_enabled)
    : ClientSocketPoolBaseHelper(max_sockets,
                                 max_sockets_per_group,
                                 unused_idle_socket_timeout,
                                 used_idle_socket_timeout,
                                 connect_job_factory.release()),
      ssl_config_service_(ssl_config_service) {
  if (connect_backup_jobs_enabled)
    EnableConnectBackupJobs();
  if (ssl_config_service_)
    ssl_config_service_->AddObserver(this);
}

void TransportClientSocketPool::OnSSLConfigChanged() {
  // When the user changes the SSL config, flush all idle sockets so they won't
  // get re-used.
  FlushWithError(ERR_NETWORK_CHANGED);
}

}  // namespace net
