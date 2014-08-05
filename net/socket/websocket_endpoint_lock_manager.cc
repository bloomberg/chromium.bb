// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/websocket_endpoint_lock_manager.h"

#include <utility>

#include "base/logging.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"

namespace net {

WebSocketEndpointLockManager::Waiter::~Waiter() {
  if (next()) {
    DCHECK(previous());
    RemoveFromList();
  }
}

WebSocketEndpointLockManager* WebSocketEndpointLockManager::GetInstance() {
  return Singleton<WebSocketEndpointLockManager>::get();
}

int WebSocketEndpointLockManager::LockEndpoint(const IPEndPoint& endpoint,
                                               Waiter* waiter) {
  LockInfoMap::value_type insert_value(endpoint, LockInfo());
  std::pair<LockInfoMap::iterator, bool> rv =
      lock_info_map_.insert(insert_value);
  LockInfo& lock_info_in_map = rv.first->second;
  if (rv.second) {
    DVLOG(3) << "Locking endpoint " << endpoint.ToString();
    lock_info_in_map.queue.reset(new LockInfo::WaiterQueue);
    return OK;
  }
  DVLOG(3) << "Waiting for endpoint " << endpoint.ToString();
  lock_info_in_map.queue->Append(waiter);
  return ERR_IO_PENDING;
}

void WebSocketEndpointLockManager::RememberSocket(StreamSocket* socket,
                                                  const IPEndPoint& endpoint) {
  LockInfoMap::iterator lock_info_it = lock_info_map_.find(endpoint);
  CHECK(lock_info_it != lock_info_map_.end());
  bool inserted =
      socket_lock_info_map_.insert(SocketLockInfoMap::value_type(
                                       socket, lock_info_it)).second;
  DCHECK(inserted);
  DCHECK(!lock_info_it->second.socket);
  lock_info_it->second.socket = socket;
  DVLOG(3) << "Remembered (StreamSocket*)" << socket << " for "
           << endpoint.ToString() << " (" << socket_lock_info_map_.size()
           << " socket(s) remembered)";
}

void WebSocketEndpointLockManager::UnlockSocket(StreamSocket* socket) {
  SocketLockInfoMap::iterator socket_it = socket_lock_info_map_.find(socket);
  if (socket_it == socket_lock_info_map_.end())
    return;

  LockInfoMap::iterator lock_info_it = socket_it->second;

  DVLOG(3) << "Unlocking (StreamSocket*)" << socket << " for "
           << lock_info_it->first.ToString() << " ("
           << socket_lock_info_map_.size() << " socket(s) left)";
  socket_lock_info_map_.erase(socket_it);
  DCHECK(socket == lock_info_it->second.socket);
  lock_info_it->second.socket = NULL;
  UnlockEndpointByIterator(lock_info_it);
}

void WebSocketEndpointLockManager::UnlockEndpoint(const IPEndPoint& endpoint) {
  LockInfoMap::iterator lock_info_it = lock_info_map_.find(endpoint);
  if (lock_info_it == lock_info_map_.end())
    return;

  UnlockEndpointByIterator(lock_info_it);
}

bool WebSocketEndpointLockManager::IsEmpty() const {
  return lock_info_map_.empty() && socket_lock_info_map_.empty();
}

WebSocketEndpointLockManager::LockInfo::LockInfo() : socket(NULL) {}
WebSocketEndpointLockManager::LockInfo::~LockInfo() {
  DCHECK(!socket);
}

WebSocketEndpointLockManager::LockInfo::LockInfo(const LockInfo& rhs)
    : socket(rhs.socket) {
  DCHECK(!rhs.queue);
}

WebSocketEndpointLockManager::WebSocketEndpointLockManager() {}

WebSocketEndpointLockManager::~WebSocketEndpointLockManager() {
  DCHECK(lock_info_map_.empty());
  DCHECK(socket_lock_info_map_.empty());
}

void WebSocketEndpointLockManager::UnlockEndpointByIterator(
    LockInfoMap::iterator lock_info_it) {
  if (lock_info_it->second.socket)
    EraseSocket(lock_info_it);
  LockInfo::WaiterQueue* queue = lock_info_it->second.queue.get();
  DCHECK(queue);
  if (queue->empty()) {
    DVLOG(3) << "Unlocking endpoint " << lock_info_it->first.ToString();
    lock_info_map_.erase(lock_info_it);
    return;
  }

  DVLOG(3) << "Unlocking endpoint " << lock_info_it->first.ToString()
           << " and activating next waiter";
  Waiter* next_job = queue->head()->value();
  next_job->RemoveFromList();
  // This must be last to minimise the excitement caused by re-entrancy.
  next_job->GotEndpointLock();
}

void WebSocketEndpointLockManager::EraseSocket(
    LockInfoMap::iterator lock_info_it) {
  DVLOG(3) << "Removing (StreamSocket*)" << lock_info_it->second.socket
           << " for " << lock_info_it->first.ToString() << " ("
           << socket_lock_info_map_.size() << " socket(s) left)";
  size_t erased = socket_lock_info_map_.erase(lock_info_it->second.socket);
  DCHECK_EQ(1U, erased);
  lock_info_it->second.socket = NULL;
}

}  // namespace net
