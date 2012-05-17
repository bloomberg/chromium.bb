// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CURVECP_PACKETIZER_H_
#define NET_CURVECP_PACKETIZER_H_
#pragma once

#include "base/basictypes.h"
#include "net/base/completion_callback.h"
#include "net/curvecp/connection_key.h"

namespace net {

class IPEndPoint;

class Packetizer {
 public:
  class Listener {
   public:
    virtual ~Listener() {}
    // Callback for new connections.
    virtual void OnConnection(ConnectionKey key) = 0;
    virtual void OnMessage(Packetizer* packetizer,
                           ConnectionKey key,
                           unsigned char* msg,
                           size_t length)  = 0;
  };

  virtual ~Packetizer() {}

  // Send a message on a connection.
  virtual int SendMessage(ConnectionKey key,
                          const char* data,
                          size_t length,
                          const CompletionCallback& callback) = 0;

  // Close an existing connection.
  virtual void Close(ConnectionKey key) = 0;

  virtual int GetPeerAddress(IPEndPoint* addresses) const = 0;

  // Returns the current maximum message size which can be fit into the next
  // message payload to be sent on the packetizer.
  virtual int max_message_payload() const = 0;
};

}  // namespace net

#endif  // NET_CURVECP_PACKETIZER_H_
