// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_HOST_TCP_H_
#define CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_HOST_TCP_H_

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "content/browser/renderer_host/p2p/socket_host.h"
#include "content/common/p2p_sockets.h"
#include "net/base/completion_callback.h"
#include "net/base/ip_endpoint.h"

namespace net {
class ClientSocket;
class DrainableIOBuffer;
class GrowableIOBuffer;
}  // namespace net

class P2PSocketHostTcp : public P2PSocketHost {
 public:
  P2PSocketHostTcp(IPC::Message::Sender* message_sender,
                   int routing_id, int id);
  virtual ~P2PSocketHostTcp();

  bool InitAccepted(const net::IPEndPoint& remote_address,
                    net::ClientSocket* socket);

  // P2PSocketHost overrides.
  virtual bool Init(const net::IPEndPoint& local_address,
                    const net::IPEndPoint& remote_address) OVERRIDE;
  virtual void Send(const net::IPEndPoint& to,
                    const std::vector<char>& data) OVERRIDE;
  virtual P2PSocketHost* AcceptIncomingTcpConnection(
      const net::IPEndPoint& remote_address, int id) OVERRIDE;

 private:
  void OnError();

  void DoRead();
  void DidCompleteRead(int result);
  void OnPacket(std::vector<char>& data);

  void DoWrite();

  // Callbacks for Connect(), Read() and Write().
  void OnConnected(int result);
  void OnRead(int result);
  void OnWritten(int result);

  net::IPEndPoint remote_address_;

  scoped_ptr<net::ClientSocket> socket_;
  scoped_refptr<net::GrowableIOBuffer> read_buffer_;
  scoped_refptr<net::DrainableIOBuffer> write_buffer_;

  bool authorized_;

  net::CompletionCallbackImpl<P2PSocketHostTcp> connect_callback_;
  net::CompletionCallbackImpl<P2PSocketHostTcp> read_callback_;
  net::CompletionCallbackImpl<P2PSocketHostTcp> write_callback_;

  DISALLOW_COPY_AND_ASSIGN(P2PSocketHostTcp);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_HOST_TCP_H_
