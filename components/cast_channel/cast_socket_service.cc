// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_channel/cast_socket_service.h"

#include "base/memory/ptr_util.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace cast_channel {

int CastSocketService::last_channel_id_ = 0;

CastSocketService::CastSocketService()
    : RefcountedKeyedService(
          BrowserThread::GetTaskRunnerForThread(BrowserThread::IO)) {
  DETACH_FROM_THREAD(thread_checker_);
}

CastSocketService::~CastSocketService() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

int CastSocketService::AddSocket(std::unique_ptr<CastSocket> socket) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(socket);
  int id = ++last_channel_id_;
  socket->set_id(id);
  sockets_.insert(std::make_pair(id, std::move(socket)));
  return id;
}

std::unique_ptr<CastSocket> CastSocketService::RemoveSocket(int channel_id) {
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

CastSocket* CastSocketService::GetSocket(int channel_id) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(channel_id > 0);
  const auto& socket_it = sockets_.find(channel_id);
  return socket_it == sockets_.end() ? nullptr : socket_it->second.get();
}

CastSocket::Observer* CastSocketService::GetObserver(const std::string& id) {
  auto it = socket_observer_map_.find(id);
  return it == socket_observer_map_.end() ? nullptr : it->second.get();
}

CastSocket::Observer* CastSocketService::AddObserver(
    const std::string& id,
    std::unique_ptr<CastSocket::Observer> observer) {
  CastSocket::Observer* observer_ptr = observer.get();
  socket_observer_map_.insert(std::make_pair(id, std::move(observer)));
  return observer_ptr;
}

void CastSocketService::ShutdownOnUIThread() {}

}  // namespace cast_channel
