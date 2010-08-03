// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_CLIENT_SOCKET_H_
#define NET_SOCKET_CLIENT_SOCKET_H_
#pragma once

#include "net/socket/socket.h"

namespace net {

class AddressList;
class BoundNetLog;

class ClientSocket : public Socket {
 public:
  ClientSocket();

  // Destructor emits statistics for this socket's lifetime.
  virtual ~ClientSocket();

  // Set the annotation to indicate this socket was created for speculative
  // reasons.  Note that if the socket was used before calling this method, then
  // the call will be ignored (no annotation will be added).
  void SetSubresourceSpeculation();
  void SetOmniboxSpeculation();

  // Establish values of was_ever_connected_ and was_used_to_transmit_data_.
  // The argument indicates if the socket's state, as reported by a
  // ClientSocketHandle::is_reused(), should show that the socket has already
  // been used to transmit data.
  // This is typically called when a transaction finishes, and
  // ClientSocketHandle is being destroyed.  Calling at that point it allows us
  // to aggregates the impact of that connect job into this instance.
  void UpdateConnectivityState(bool is_reused);

  // Called to establish a connection.  Returns OK if the connection could be
  // established synchronously.  Otherwise, ERR_IO_PENDING is returned and the
  // given callback will run asynchronously when the connection is established
  // or when an error occurs.  The result is some other error code if the
  // connection could not be established.
  //
  // The socket's Read and Write methods may not be called until Connect
  // succeeds.
  //
  // It is valid to call Connect on an already connected socket, in which case
  // OK is simply returned.
  //
  // Connect may also be called again after a call to the Disconnect method.
  //
  virtual int Connect(CompletionCallback* callback) = 0;

  // Called to disconnect a socket.  Does nothing if the socket is already
  // disconnected.  After calling Disconnect it is possible to call Connect
  // again to establish a new connection.
  //
  // If IO (Connect, Read, or Write) is pending when the socket is
  // disconnected, the pending IO is cancelled, and the completion callback
  // will not be called.
  virtual void Disconnect() = 0;

  // Called to test if the connection is still alive.  Returns false if a
  // connection wasn't established or the connection is dead.
  virtual bool IsConnected() const = 0;

  // Called to test if the connection is still alive and idle.  Returns false
  // if a connection wasn't established, the connection is dead, or some data
  // have been received.
  virtual bool IsConnectedAndIdle() const = 0;

  // Copies the peer address to |address| and returns a network error code.
  virtual int GetPeerAddress(AddressList* address) const = 0;

  // Gets the NetLog for this socket.
  virtual const BoundNetLog& NetLog() const = 0;

 private:
  // Publish histogram to help evaluate preconnection utilization.
  void EmitPreconnectionHistograms() const;

  // Indicate if any ClientSocketHandle that used this socket was connected as
  // would be indicated by the IsConnected() method.  This variable set by a
  // ClientSocketHandle before releasing this ClientSocket.
  bool was_ever_connected_;

  // Indicate if this socket was first created for speculative use, and identify
  // the motivation.
  bool omnibox_speculation_;
  bool subresource_speculation_;

  // Indicate if this socket was ever used. This state is set by a
  // ClientSocketHandle before releasing this ClientSocket.
  bool was_used_to_transmit_data_;
};

}  // namespace net

#endif  // NET_SOCKET_CLIENT_SOCKET_H_
