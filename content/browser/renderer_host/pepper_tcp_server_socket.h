// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PEPPER_TCP_SERVER_SOCKET_H_
#define CONTENT_BROWSER_RENDERER_HOST_PEPPER_TCP_SERVER_SOCKET_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

class PepperMessageFilter;
struct PP_NetAddress_Private;

namespace net {
class ServerSocket;
class StreamSocket;
}

// PepperTCPSocket is used by PepperMessageFilter to handle requests
// from the Pepper TCP server socket API
// (PPB_TCPServerSocket_Private).
class PepperTCPServerSocket {
 public:
  PepperTCPServerSocket(PepperMessageFilter* manager,
                        int32 routing_id,
                        uint32 plugin_dispatcher_id,
                        uint32 real_socket_id,
                        uint32 temp_socket_id);
  ~PepperTCPServerSocket();

  void Listen(const PP_NetAddress_Private& addr, int32 backlog);
  void Accept();

 private:
  enum State {
    BEFORE_LISTENING,
    LISTEN_IN_PROGRESS,
    LISTENING,
    ACCEPT_IN_PROGRESS,
  };

  void CancelListenRequest();
  void SendAcceptACKError();

  void OnListenCompleted(int result);
  void OnAcceptCompleted(int result);

  PepperMessageFilter* manager_;
  int32 routing_id_;
  uint32 plugin_dispatcher_id_;
  uint32 real_socket_id_;
  uint32 temp_socket_id_;

  State state_;

  scoped_ptr<net::ServerSocket> socket_;
  scoped_ptr<net::StreamSocket> socket_buffer_;

  DISALLOW_COPY_AND_ASSIGN(PepperTCPServerSocket);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_PEPPER_TCP_SERVER_SOCKET_H_
