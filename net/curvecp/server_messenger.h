// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CURVECP_SERVER_MESSENGER_H_
#define NET_CURVECP_SERVER_MESSENGER_H_
#pragma once

#include <list>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/task.h"
#include "net/curvecp/messenger.h"
#include "net/curvecp/packetizer.h"
#include "net/socket/socket.h"

namespace net {

// A specialized messenger for listening on a socket, accepting new sockets,
// and dispatching io to secondary
class ServerMessenger : public Messenger {
 public:
  class Acceptor {
   public:
    virtual ~Acceptor() {}
    virtual void OnAccept(ConnectionKey key) = 0;
  };

  ServerMessenger(Packetizer* packetizer, Acceptor* acceptor);
  ServerMessenger(ConnectionKey key,
                  Packetizer* packetizer,
                  Acceptor* acceptor);
  virtual ~ServerMessenger();

  // Override OnConnection to track incoming connections.
  virtual void OnConnection(ConnectionKey key) OVERRIDE;

 private:
  Acceptor* acceptor_;
  DISALLOW_COPY_AND_ASSIGN(ServerMessenger);
};

}  // namespace net

#endif  // NET_CURVECP_SERVER_MESSENGER_H_
