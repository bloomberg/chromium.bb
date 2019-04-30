// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/client_socket_pool.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "net/http/http_proxy_connect_job.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/socks_connect_job.h"
#include "net/socket/ssl_connect_job.h"
#include "net/socket/stream_socket.h"
#include "net/socket/transport_connect_job.h"
#include "net/socket/websocket_transport_connect_job.h"

namespace net {

namespace {

// The maximum duration, in seconds, to keep used idle persistent sockets alive.
int64_t g_used_idle_socket_timeout_s = 300;  // 5 minutes

}  // namespace

ClientSocketPool::SocketParams::SocketParams(
    std::unique_ptr<SSLConfig> ssl_config_for_origin,
    std::unique_ptr<SSLConfig> ssl_config_for_proxy,
    const OnHostResolutionCallback& resolution_callback)
    : ssl_config_for_origin_(std::move(ssl_config_for_origin)),
      ssl_config_for_proxy_(std::move(ssl_config_for_proxy)),
      resolution_callback_(resolution_callback) {}

ClientSocketPool::SocketParams::~SocketParams() = default;

scoped_refptr<ClientSocketPool::SocketParams>
ClientSocketPool::SocketParams::CreateForHttpForTesting() {
  return base::MakeRefCounted<SocketParams>(nullptr /* ssl_config_for_origin */,
                                            nullptr /* ssl_config_for_proxy */,
                                            OnHostResolutionCallback());
}

ClientSocketPool::GroupId::GroupId()
    : socket_type_(SocketType::kHttp),
      privacy_mode_(PrivacyMode::PRIVACY_MODE_DISABLED) {}

ClientSocketPool::GroupId::GroupId(const HostPortPair& destination,
                                   SocketType socket_type,
                                   PrivacyMode privacy_mode)
    : destination_(destination),
      socket_type_(socket_type),
      privacy_mode_(privacy_mode) {}

ClientSocketPool::GroupId::GroupId(const GroupId& group_id) = default;

ClientSocketPool::GroupId::~GroupId() = default;

ClientSocketPool::GroupId& ClientSocketPool::GroupId::operator=(
    const GroupId& group_id) = default;

ClientSocketPool::GroupId& ClientSocketPool::GroupId::operator=(
    GroupId&& group_id) = default;

std::string ClientSocketPool::GroupId::ToString() const {
  std::string result = destination_.ToString();
  switch (socket_type_) {
    case ClientSocketPool::SocketType::kHttp:
      break;

    case ClientSocketPool::SocketType::kSsl:
      result = "ssl/" + result;
      break;

    case ClientSocketPool::SocketType::kFtp:
      result = "ftp/" + result;
      break;
  }
  if (privacy_mode_)
    result = "pm/" + result;
  return result;
}

ClientSocketPool::~ClientSocketPool() = default;

// static
base::TimeDelta ClientSocketPool::used_idle_socket_timeout() {
  return base::TimeDelta::FromSeconds(g_used_idle_socket_timeout_s);
}

// static
void ClientSocketPool::set_used_idle_socket_timeout(base::TimeDelta timeout) {
  DCHECK_GT(timeout.InSeconds(), 0);
  g_used_idle_socket_timeout_s = timeout.InSeconds();
}

ClientSocketPool::ClientSocketPool() = default;

void ClientSocketPool::NetLogTcpClientSocketPoolRequestedSocket(
    const NetLogWithSource& net_log,
    const GroupId& group_id) {
  if (net_log.IsCapturing()) {
    // TODO(eroman): Split out the host and port parameters.
    net_log.AddEvent(NetLogEventType::TCP_CLIENT_SOCKET_POOL_REQUESTED_SOCKET,
                     base::BindRepeating(&NetLogGroupIdCallback,
                                         base::Unretained(&group_id)));
  }
}

base::Value ClientSocketPool::NetLogGroupIdCallback(
    const GroupId* group_id,
    NetLogCaptureMode /* capture_mode */) {
  base::DictionaryValue event_params;
  event_params.SetString("group_id", group_id->ToString());
  return std::move(event_params);
}

std::unique_ptr<ConnectJob> ClientSocketPool::CreateConnectJob(
    GroupId group_id,
    scoped_refptr<SocketParams> socket_params,
    const ProxyServer& proxy_server,
    const base::Optional<NetworkTrafficAnnotationTag>& proxy_annotation_tag,
    bool is_for_websockets,
    const CommonConnectJobParams* common_connect_job_params,
    RequestPriority request_priority,
    SocketTag socket_tag,
    ConnectJob::Delegate* delegate) {
  bool using_ssl = group_id.socket_type() == ClientSocketPool::SocketType::kSsl;
  return ConnectJob::CreateConnectJob(
      using_ssl, group_id.destination(), proxy_server, proxy_annotation_tag,
      socket_params->ssl_config_for_origin(),
      socket_params->ssl_config_for_proxy(), is_for_websockets,
      group_id.privacy_mode(), socket_params->resolution_callback(),
      request_priority, socket_tag, common_connect_job_params, delegate);
}

}  // namespace net
