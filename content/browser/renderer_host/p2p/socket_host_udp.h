// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_HOST_UDP_H_
#define CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_HOST_UDP_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <set>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/browser/renderer_host/p2p/socket_host.h"
#include "content/common/content_export.h"
#include "net/base/ip_endpoint.h"
#include "net/socket/diff_serv_code_point.h"
#include "net/socket/udp_server_socket.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/p2p_socket_type.h"
#include "third_party/webrtc/rtc_base/asyncpacketsocket.h"

namespace net {
class NetLog;
}  // namespace net

namespace content {

class P2PMessageThrottler;

class CONTENT_EXPORT P2PSocketHostUdp : public P2PSocketHost {
 public:
  typedef base::Callback<std::unique_ptr<net::DatagramServerSocket>(
      net::NetLog* net_log)>
      DatagramServerSocketFactory;
  P2PSocketHostUdp(P2PSocketDispatcherHost* socket_dispatcher_host,
                   network::mojom::P2PSocketClientPtr client,
                   network::mojom::P2PSocketRequest socket,
                   P2PMessageThrottler* throttler,
                   net::NetLog* net_log,
                   const DatagramServerSocketFactory& socket_factory);
  P2PSocketHostUdp(P2PSocketDispatcherHost* socket_dispatcher_host,
                   network::mojom::P2PSocketClientPtr client,
                   network::mojom::P2PSocketRequest socket,
                   P2PMessageThrottler* throttler,
                   net::NetLog* net_log);
  ~P2PSocketHostUdp() override;

  // P2PSocketHost overrides.
  bool Init(const net::IPEndPoint& local_address,
            uint16_t min_port,
            uint16_t max_port,
            const network::P2PHostAndIPEndPoint& remote_address) override;

  // network::mojom::P2PSocket implementation:
  void AcceptIncomingTcpConnection(
      const net::IPEndPoint& remote_address,
      network::mojom::P2PSocketClientPtr client,
      network::mojom::P2PSocketRequest socket) override;
  void Send(const std::vector<int8_t>& data,
            const network::P2PPacketInfo& packet_info,
            const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;
  void SetOption(network::P2PSocketOption option, int32_t value) override;

 private:
  friend class P2PSocketHostUdpTest;

  typedef std::set<net::IPEndPoint> ConnectedPeerSet;

  struct PendingPacket {
    PendingPacket(const net::IPEndPoint& to,
                  const std::vector<int8_t>& content,
                  const rtc::PacketOptions& options,
                  uint64_t id,
                  const net::NetworkTrafficAnnotationTag traffic_annotation);
    PendingPacket(const PendingPacket& other);
    ~PendingPacket();
    net::IPEndPoint to;
    scoped_refptr<net::IOBuffer> data;
    int size;
    rtc::PacketOptions packet_options;
    uint64_t id;
    const net::NetworkTrafficAnnotationTag traffic_annotation;
  };

  void OnError();

  void DoRead();
  void OnRecv(int result);
  void HandleReadResult(int result);

  void DoSend(const PendingPacket& packet);
  void OnSend(uint64_t packet_id,
              int32_t transport_sequence_number,
              base::TimeTicks send_time,
              int result);
  void HandleSendResult(uint64_t packet_id,
                        int32_t transport_sequence_number,
                        base::TimeTicks send_time,
                        int result);
  int SetSocketDiffServCodePointInternal(net::DiffServCodePoint dscp);
  static std::unique_ptr<net::DatagramServerSocket> DefaultSocketFactory(
      net::NetLog* net_log);

  std::unique_ptr<net::DatagramServerSocket> socket_;
  scoped_refptr<net::IOBuffer> recv_buffer_;
  net::IPEndPoint recv_address_;

  base::circular_deque<PendingPacket> send_queue_;
  bool send_pending_;
  net::DiffServCodePoint last_dscp_;

  // Set of peer for which we have received STUN binding request or
  // response or relay allocation request or response.
  ConnectedPeerSet connected_peers_;
  P2PMessageThrottler* throttler_;

  net::NetLog* net_log_;

  // Callback object that returns a new socket when invoked.
  DatagramServerSocketFactory socket_factory_;

  DISALLOW_COPY_AND_ASSIGN(P2PSocketHostUdp);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_HOST_UDP_H_
