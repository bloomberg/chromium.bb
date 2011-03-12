// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_HOST_H_

#include "content/common/p2p_sockets.h"

#include "net/base/ip_endpoint.h"

class P2PSocketsHost;

// Base class for P2P sockets used by P2PSocketsHost.
class P2PSocketHost {
 public:
  // Creates P2PSocketHost for the current platform. Must be
  // implemented in a platform-specific file.
  static P2PSocketHost* Create(P2PSocketsHost* host, int routing_id, int id,
                               P2PSocketType type);

  virtual ~P2PSocketHost();

  // Initalizes the socket. Returns false when initiazations fails.
  virtual bool Init() = 0;

  // Sends |data| on the socket to |socket_address|.
  virtual void Send(const net::IPEndPoint& socket_address,
                    const std::vector<char>& data) = 0;

 protected:
  P2PSocketHost(P2PSocketsHost* host, int routing_id, int id);

  P2PSocketsHost* host_;
  int routing_id_;
  int id_;

  DISALLOW_COPY_AND_ASSIGN(P2PSocketHost);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_HOST_H_
