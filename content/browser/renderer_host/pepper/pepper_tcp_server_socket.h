// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_TCP_SERVER_SOCKET_H_
#define CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_TCP_SERVER_SOCKET_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ppapi/c/pp_resource.h"

struct PP_NetAddress_Private;

namespace net {
class ServerSocket;
class StreamSocket;
}

namespace content {
class PepperMessageFilter;

// PepperTCPSocket is used by PepperMessageFilter to handle requests
// from the Pepper TCP server socket API
// (PPB_TCPServerSocket_Private).
class PepperTCPServerSocket {
 public:
  PepperTCPServerSocket(PepperMessageFilter* manager,
                        int32 routing_id,
                        uint32 plugin_dispatcher_id,
                        PP_Resource socket_resource,
                        uint32 socket_id);
  ~PepperTCPServerSocket();

  void Listen(const PP_NetAddress_Private& addr, int32 backlog);
  void Accept(int32 tcp_client_socket_routing_id);

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
  void OnAcceptCompleted(int32 tcp_client_socket_routing_id,
                         int result);

  PepperMessageFilter* manager_;
  int32 routing_id_;
  uint32 plugin_dispatcher_id_;
  // socket_resource_ is used only as an ID on the browser side. So we
  // don't hold the ref to this resource and it may become invalid at
  // the renderer/plugin side.
  PP_Resource socket_resource_;
  uint32 socket_id_;

  State state_;

  scoped_ptr<net::ServerSocket> socket_;
  scoped_ptr<net::StreamSocket> socket_buffer_;

  DISALLOW_COPY_AND_ASSIGN(PepperTCPServerSocket);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_TCP_SERVER_SOCKET_H_
