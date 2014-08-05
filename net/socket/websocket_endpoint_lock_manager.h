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
  // UnlockSocket() or UnlockEndpoint() is called. The |socket| pointer must not
  // be deleted between the call to RememberSocket() and the call to
  // UnlockSocket().
  void RememberSocket(StreamSocket* socket, const IPEndPoint& endpoint);

  // Releases the lock on the endpoint that was associated with |socket| by
  // RememberSocket(). If appropriate, triggers the next socket connection.
  // Should be called exactly once for each |socket| that was passed to
  // RememberSocket(). Does nothing if UnlockEndpoint() has been called since
  // the call to RememberSocket().
  void UnlockSocket(StreamSocket* socket);

  // Releases the lock on |endpoint|. Does nothing if |endpoint| is not locked.
  // Removes any socket association that was recorded with RememberSocket(). If
  // appropriate, calls |waiter->GotEndpointLock()|.
  void UnlockEndpoint(const IPEndPoint& endpoint);

  // Checks that |lock_info_map_| and |socket_lock_info_map_| are empty. For
  // tests.
  bool IsEmpty() const;

 private:
  struct LockInfo {
    typedef base::LinkedList<Waiter> WaiterQueue;

    LockInfo();
    ~LockInfo();

    // This object must be copyable to be placed in the map, but it cannot be
    // copied after |queue| has been assigned to.
    LockInfo(const LockInfo& rhs);

    // Not used.
    LockInfo& operator=(const LockInfo& rhs);

    // Must be NULL to copy this object into the map. Must be set to non-NULL
    // after the object is inserted into the map then point to the same list
    // until this object is deleted.
    scoped_ptr<WaiterQueue> queue;

    // This pointer is only used to identify the last instance of StreamSocket
    // that was passed to RememberSocket() for this endpoint. It should only be
    // compared with other pointers. It is never dereferenced and not owned. It
    // is non-NULL if RememberSocket() has been called for this endpoint since
    // the last call to UnlockSocket() or UnlockEndpoint().
    StreamSocket* socket;
  };

  // SocketLockInfoMap requires std::map iterator semantics for LockInfoMap
  // (ie. that the iterator will remain valid as long as the entry is not
  // deleted).
  typedef std::map<IPEndPoint, LockInfo> LockInfoMap;
  typedef std::map<StreamSocket*, LockInfoMap::iterator> SocketLockInfoMap;

  WebSocketEndpointLockManager();
  ~WebSocketEndpointLockManager();

  void UnlockEndpointByIterator(LockInfoMap::iterator lock_info_it);
  void EraseSocket(LockInfoMap::iterator lock_info_it);

  // If an entry is present in the map for a particular endpoint, then that
  // endpoint is locked. If LockInfo.queue is non-empty, then one or more
  // Waiters are waiting for the lock.
  LockInfoMap lock_info_map_;

  // Store sockets remembered by RememberSocket() and not yet unlocked by
  // UnlockSocket() or UnlockEndpoint(). Every entry in this map always
  // references a live entry in lock_info_map_, and the LockInfo::socket member
  // is non-NULL if and only if there is an entry in this map for the socket.
  SocketLockInfoMap socket_lock_info_map_;

  friend struct DefaultSingletonTraits<WebSocketEndpointLockManager>;

  DISALLOW_COPY_AND_ASSIGN(WebSocketEndpointLockManager);
};

}  // namespace net

#endif  // NET_SOCKET_WEBSOCKET_ENDPOINT_LOCK_MANAGER_H_
