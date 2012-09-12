// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_HOST_UDP_H_
#define CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_HOST_UDP_H_

#include <deque>
#include <set>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "content/browser/renderer_host/p2p/socket_host.h"
#include "content/common/content_export.h"
#include "content/common/p2p_sockets.h"
#include "ipc/ipc_sender.h"
#include "net/base/ip_endpoint.h"
#include "net/udp/udp_server_socket.h"

namespace content {

class CONTENT_EXPORT P2PSocketHostUdp : public P2PSocketHost {
 public:
  P2PSocketHostUdp(IPC::Sender* message_sender, int id);
  virtual ~P2PSocketHostUdp();

  // P2PSocketHost overrides.
  virtual bool Init(const net::IPEndPoint& local_address,
                    const net::IPEndPoint& remote_address) OVERRIDE;
  virtual void Send(const net::IPEndPoint& to,
                    const std::vector<char>& data) OVERRIDE;
  virtual P2PSocketHost* AcceptIncomingTcpConnection(
      const net::IPEndPoint& remote_address, int id) OVERRIDE;

 private:
  friend class P2PSocketHostUdpTest;

  typedef std::set<net::IPEndPoint> ConnectedPeerSet;

  struct PendingPacket {
    PendingPacket(const net::IPEndPoint& to, const std::vector<char>& content);
    ~PendingPacket();
    net::IPEndPoint to;
    scoped_refptr<net::IOBuffer> data;
    int size;
  };

  void OnError();
  void DoRead();
  void DoSend(const PendingPacket& packet);
  void DidCompleteRead(int result);

  // Callbacks for RecvFrom() and SendTo().
  void OnRecv(int result);
  void OnSend(int result);

  scoped_ptr<net::DatagramServerSocket> socket_;
  scoped_refptr<net::IOBuffer> recv_buffer_;
  net::IPEndPoint recv_address_;

  std::deque<PendingPacket> send_queue_;
  int send_queue_bytes_;
  bool send_pending_;

  // Set of peer for which we have received STUN binding request or
  // response or relay allocation request or response.
  ConnectedPeerSet connected_peers_;

  DISALLOW_COPY_AND_ASSIGN(P2PSocketHostUdp);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_HOST_UDP_H_
