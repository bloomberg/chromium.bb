// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/cast_channel/cast_socket_service.h"

#include "base/memory/ptr_util.h"

namespace extensions {
namespace api {
namespace cast_channel {

int CastSocketRegistry::last_channel_id_ = 0;

CastSocketRegistry::CastSocketRegistry() = default;

CastSocketRegistry::~CastSocketRegistry() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

int CastSocketRegistry::AddSocket(std::unique_ptr<CastSocket> socket) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(socket);
  int id = ++last_channel_id_;
  socket->set_id(id);
  sockets_.insert(std::make_pair(id, std::move(socket)));
  return id;
}

std::unique_ptr<CastSocket> CastSocketRegistry::RemoveSocket(int channel_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(channel_id > 0);
  auto socket_it = sockets_.find(channel_id);

  std::unique_ptr<CastSocket> socket;
  if (socket_it != sockets_.end()) {
    socket = std::move(socket_it->second);
    sockets_.erase(socket_it);
  }
  return socket;
}

CastSocket* CastSocketRegistry::GetSocket(int channel_id) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(channel_id > 0);
  const auto& socket_it = sockets_.find(channel_id);
  return socket_it == sockets_.end() ? nullptr : socket_it->second.get();
}

CastSocketService::CastSocketService() = default;
CastSocketService::~CastSocketService() = default;

CastSocketRegistry* CastSocketService::GetOrCreateSocketRegistry() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (!sockets_)
    sockets_.reset(new CastSocketRegistry());
  return sockets_.get();
}

}  // namespace cast_channel
}  // namespace api
}  // namespace extensions
