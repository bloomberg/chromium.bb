// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/client_socket_pool.h"

#include <utility>

#include "base/logging.h"
#include "net/socket/stream_socket.h"

namespace {

// The maximum duration, in seconds, to keep used idle persistent sockets alive.
int64_t g_used_idle_socket_timeout_s = 300;  // 5 minutes

}  // namespace

namespace net {

ClientSocketPool::GroupId::GroupId()
    : socket_type_(SocketType::kHttp), privacy_mode_(false) {}

ClientSocketPool::GroupId::GroupId(const HostPortPair& destination,
                                   SocketType socket_type,
                                   bool privacy_mode)
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

    case ClientSocketPool::SocketType::kSslVersionInterferenceProbe:
      result = "version-interference-probe/ssl/" + result;
      break;

    case ClientSocketPool::SocketType::kFtp:
      result = "ftp/" + result;
      break;
  }
  if (privacy_mode_)
    result = "pm/" + result;
  return result;
}

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

ClientSocketPool::~ClientSocketPool() = default;

}  // namespace net
