// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_HOST_TCP_H_
#define CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_HOST_TCP_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "content/browser/renderer_host/p2p/socket_host.h"
#include "content/common/p2p_sockets.h"
#include "net/base/completion_callback.h"
#include "net/base/ip_endpoint.h"

namespace net {
class DrainableIOBuffer;
class GrowableIOBuffer;
class StreamSocket;
}  // namespace net

namespace content {

class CONTENT_EXPORT P2PSocketHostTcp : public P2PSocketHost {
 public:
  P2PSocketHostTcp(IPC::Sender* message_sender, int id);
  virtual ~P2PSocketHostTcp();

  bool InitAccepted(const net::IPEndPoint& remote_address,
                    net::StreamSocket* socket);

  // P2PSocketHost overrides.
  virtual bool Init(const net::IPEndPoint& local_address,
                    const net::IPEndPoint& remote_address) OVERRIDE;
  virtual void Send(const net::IPEndPoint& to,
                    const std::vector<char>& data) OVERRIDE;
  virtual P2PSocketHost* AcceptIncomingTcpConnection(
      const net::IPEndPoint& remote_address, int id) OVERRIDE;

 private:
  friend class P2PSocketHostTcpTest;
  friend class P2PSocketHostTcpServerTest;

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

  scoped_ptr<net::StreamSocket> socket_;
  scoped_refptr<net::GrowableIOBuffer> read_buffer_;
  scoped_refptr<net::DrainableIOBuffer> write_buffer_;

  bool connected_;

  DISALLOW_COPY_AND_ASSIGN(P2PSocketHostTcp);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_HOST_TCP_H_
