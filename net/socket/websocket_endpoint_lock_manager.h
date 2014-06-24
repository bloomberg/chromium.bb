// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_WEBSOCKET_ENDPOINT_LOCK_MANAGER_H_
#define NET_SOCKET_WEBSOCKET_ENDPOINT_LOCK_MANAGER_H_

#include <map>

#include "base/containers/linked_list.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_export.h"
#include "net/socket/websocket_transport_client_socket_pool.h"

namespace net {

class StreamSocket;

class NET_EXPORT_PRIVATE WebSocketEndpointLockManager {
 public:
  class NET_EXPORT_PRIVATE Waiter : public base::LinkNode<Waiter> {
   public:
    // If the node is in a list, removes it.
    virtual ~Waiter();

    virtual void GotEndpointLock() = 0;
  };

  static WebSocketEndpointLockManager* GetInstance();

  // Returns OK if lock was acquired immediately, ERR_IO_PENDING if not. If the
  // lock was not acquired, then |waiter->GotEndpointLock()| will be called when
  // it is. A Waiter automatically removes itself from the list of waiters when
  // its destructor is called.
  int LockEndpoint(const IPEndPoint& endpoint, Waiter* waiter);

  // Records the IPEndPoint associated with a particular socket. This is
  // necessary because TCPClientSocket refuses to return the PeerAddress after
  // the connection is disconnected. The association will be forgotten when
  // UnlockSocket() is called. The |socket| pointer must not be deleted between
  // the call to RememberSocket() and the call to UnlockSocket().
  void RememberSocket(StreamSocket* socket, const IPEndPoint& endpoint);

  // Releases the lock on an endpoint, and, if appropriate, triggers the next
  // socket connection. For a successful WebSocket connection, this method will
  // be called once when the handshake completes, and again when the connection
  // is closed. Calls after the first are safely ignored.
  void UnlockSocket(StreamSocket* socket);

  // Releases the lock on |endpoint|. If RememberSocket() has been called for
  // this endpoint, then UnlockSocket() must be used instead of this method.
  void UnlockEndpoint(const IPEndPoint& endpoint);

  // Checks that |endpoint_waiter_map_| and |socket_endpoint_map_| are
  // empty. For tests.
  bool IsEmpty() const;

 private:
  typedef base::LinkedList<Waiter> ConnectJobQueue;
  typedef std::map<IPEndPoint, ConnectJobQueue*> EndPointWaiterMap;
  typedef std::map<StreamSocket*, IPEndPoint> SocketEndPointMap;

  WebSocketEndpointLockManager();
  ~WebSocketEndpointLockManager();

  // If an entry is present in the map for a particular endpoint, then that
  // endpoint is locked. If the list is non-empty, then one or more Waiters are
  // waiting for the lock.
  EndPointWaiterMap endpoint_waiter_map_;

  // Store sockets remembered by RememberSocket() and not yet unlocked by
  // UnlockSocket().
  SocketEndPointMap socket_endpoint_map_;

  friend struct DefaultSingletonTraits<WebSocketEndpointLockManager>;

  DISALLOW_COPY_AND_ASSIGN(WebSocketEndpointLockManager);
};

}  // namespace net

#endif  // NET_SOCKET_WEBSOCKET_ENDPOINT_LOCK_MANAGER_H_
