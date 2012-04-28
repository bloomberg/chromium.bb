// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Abstract socket server that handles IO asynchronously in the specified
// MessageLoop.

#ifndef NET_BASE_LISTEN_SOCKET_H_
#define NET_BASE_LISTEN_SOCKET_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "net/base/net_export.h"

namespace net {

// Defines a socket interface for a server.
class NET_EXPORT ListenSocket
    : public base::RefCountedThreadSafe<ListenSocket> {
 public:
  // TODO(erikkay): this delegate should really be split into two parts
  // to split up the listener from the connected socket.  Perhaps this class
  // should be split up similarly.
  class ListenSocketDelegate {
   public:
    // server is the original listening Socket, connection is the new
    // Socket that was created.  Ownership of connection is transferred
    // to the delegate with this call.
    virtual void DidAccept(ListenSocket *server,
                           ListenSocket *connection) = 0;
    virtual void DidRead(ListenSocket *connection,
                         const char* data,
                         int len) = 0;
    virtual void DidClose(ListenSocket *sock) = 0;

   protected:
    virtual ~ListenSocketDelegate() {}
  };

  // Send data to the socket.
  void Send(const char* bytes, int len, bool append_linefeed = false);
  void Send(const std::string& str, bool append_linefeed = false);

 protected:
  ListenSocket(ListenSocketDelegate* del);
  virtual ~ListenSocket();

  virtual void SendInternal(const char* bytes, int len) = 0;

  ListenSocketDelegate* const socket_delegate_;

 private:
  friend class base::RefCountedThreadSafe<ListenSocket>;

  DISALLOW_COPY_AND_ASSIGN(ListenSocket);
};

}  // namespace net

#endif  // NET_BASE_LISTEN_SOCKET_H_
