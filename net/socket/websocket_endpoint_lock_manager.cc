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
  EndPointWaiterMap::value_type insert_value(endpoint, NULL);
  std::pair<EndPointWaiterMap::iterator, bool> rv =
      endpoint_waiter_map_.insert(insert_value);
  if (rv.second) {
    DVLOG(3) << "Locking endpoint " << endpoint.ToString();
    rv.first->second = new ConnectJobQueue;
    return OK;
  }
  DVLOG(3) << "Waiting for endpoint " << endpoint.ToString();
  rv.first->second->Append(waiter);
  return ERR_IO_PENDING;
}

void WebSocketEndpointLockManager::RememberSocket(StreamSocket* socket,
                                                  const IPEndPoint& endpoint) {
  bool inserted = socket_endpoint_map_.insert(SocketEndPointMap::value_type(
                                                  socket, endpoint)).second;
  DCHECK(inserted);
  DCHECK(endpoint_waiter_map_.find(endpoint) != endpoint_waiter_map_.end());
  DVLOG(3) << "Remembered (StreamSocket*)" << socket << " for "
           << endpoint.ToString() << " (" << socket_endpoint_map_.size()
           << " sockets remembered)";
}

void WebSocketEndpointLockManager::UnlockSocket(StreamSocket* socket) {
  SocketEndPointMap::iterator socket_it = socket_endpoint_map_.find(socket);
  if (socket_it == socket_endpoint_map_.end()) {
    DVLOG(3) << "Ignoring request to unlock already-unlocked socket"
                "(StreamSocket*)" << socket;
    return;
  }
  const IPEndPoint& endpoint = socket_it->second;
  DVLOG(3) << "Unlocking (StreamSocket*)" << socket << " for "
           << endpoint.ToString() << " (" << socket_endpoint_map_.size()
           << " sockets left)";
  UnlockEndpoint(endpoint);
  socket_endpoint_map_.erase(socket_it);
}

void WebSocketEndpointLockManager::UnlockEndpoint(const IPEndPoint& endpoint) {
  EndPointWaiterMap::iterator found_it = endpoint_waiter_map_.find(endpoint);
  CHECK(found_it != endpoint_waiter_map_.end());  // Security critical
  ConnectJobQueue* queue = found_it->second;
  if (queue->empty()) {
    DVLOG(3) << "Unlocking endpoint " << endpoint.ToString();
    delete queue;
    endpoint_waiter_map_.erase(found_it);
  } else {
    DVLOG(3) << "Unlocking endpoint " << endpoint.ToString()
             << " and activating next waiter";
    Waiter* next_job = queue->head()->value();
    next_job->RemoveFromList();
    next_job->GotEndpointLock();
  }
}

bool WebSocketEndpointLockManager::IsEmpty() const {
  return endpoint_waiter_map_.empty() && socket_endpoint_map_.empty();
}

WebSocketEndpointLockManager::WebSocketEndpointLockManager() {}

WebSocketEndpointLockManager::~WebSocketEndpointLockManager() {
  DCHECK(endpoint_waiter_map_.empty());
  DCHECK(socket_endpoint_map_.empty());
}

}  // namespace net
