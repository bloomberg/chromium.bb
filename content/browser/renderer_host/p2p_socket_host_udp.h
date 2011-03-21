// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_HOST_UDP_H_
#define CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_HOST_UDP_H_

#include "content/common/p2p_sockets.h"

#include "base/message_loop.h"
#include "content/browser/renderer_host/p2p_socket_host.h"
#include "net/udp/udp_server_socket.h"

class P2PSocketHostUdp : public P2PSocketHost {
 public:
  P2PSocketHostUdp(P2PSocketsHost* host, int routing_id, int id);
  virtual ~P2PSocketHostUdp();

  // P2PSocketHost overrides.
  virtual bool Init(const net::IPEndPoint& local_address) OVERRIDE;
  virtual void Send(const net::IPEndPoint& socket_address,
                    const std::vector<char>& data) OVERRIDE;

 private:
  enum State {
    STATE_UNINITIALIZED,
    STATE_OPEN,
    STATE_ERROR,
  };

  void OnError();
  void DoRead();
  void DidCompleteRead(int result);

  // Callbacks for RecvFrom() and SendTo().
  void OnRecv(int result);
  void OnSend(int result);

  State state_;
  scoped_ptr<net::DatagramServerSocket> socket_;
  scoped_refptr<net::IOBuffer> recv_buffer_;
  net::IPEndPoint recv_address_;
  bool send_pending_;

  net::CompletionCallbackImpl<P2PSocketHostUdp> recv_callback_;
  net::CompletionCallbackImpl<P2PSocketHostUdp> send_callback_;

  DISALLOW_COPY_AND_ASSIGN(P2PSocketHostUdp);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_HOST_UDP_H_
