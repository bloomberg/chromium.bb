// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_P2P_SOCKET_CLIENT_IMPL_H_
#define CONTENT_RENDERER_P2P_SOCKET_CLIENT_IMPL_H_

#include <stdint.h>

#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "content/renderer/p2p/socket_client.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "net/base/ip_endpoint.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/p2p_socket_type.h"
#include "services/network/public/mojom/p2p.mojom.h"

namespace base {
class TimeTicks;
}  // namespace base

namespace content {

class P2PSocketDispatcher;

// P2P socket that routes all calls over Mojo.
//
// The object runs on the WebRTC worker thread.
class P2PSocketClientImpl : public P2PSocketClient,
                            public network::mojom::P2PSocketClient {
 public:
  P2PSocketClientImpl(
      P2PSocketDispatcher* dispatcher,
      const net::NetworkTrafficAnnotationTag& traffic_annotation);
  ~P2PSocketClientImpl() override;

  // Initialize socket of the specified |type| and connected to the
  // specified |address|. |address| matters only when |type| is set to
  // P2P_SOCKET_TCP_CLIENT.
  virtual void Init(network::P2PSocketType type,
                    const net::IPEndPoint& local_address,
                    uint16_t min_port,
                    uint16_t max_port,
                    const network::P2PHostAndIPEndPoint& remote_address,
                    P2PSocketClientDelegate* delegate);

  // Send the |data| to the |address| using Differentiated Services Code Point
  // |dscp|. Return value is the unique packet_id for this packet.
  uint64_t Send(const net::IPEndPoint& address,
                const std::vector<int8_t>& data,
                const rtc::PacketOptions& options) override;

  // Setting socket options.
  void SetOption(network::P2PSocketOption option, int value) override;

  // Must be called before the socket is destroyed. The delegate may
  // not be called after |closed_task| is executed.
  void Close() override;

  int GetSocketID() const override;

  void SetDelegate(P2PSocketClientDelegate* delegate) override;

 private:
  enum State {
    STATE_UNINITIALIZED,
    STATE_OPENING,
    STATE_OPEN,
    STATE_CLOSED,
    STATE_ERROR,
  };

  friend class P2PSocketDispatcher;

  // Helper function to be called by Send to handle different threading
  // condition.
  void SendWithPacketId(const net::IPEndPoint& address,
                        const std::vector<int8_t>& data,
                        const rtc::PacketOptions& options,
                        uint64_t packet_id);

  // network::mojom::P2PSocketClient interface.
  void SocketCreated(const net::IPEndPoint& local_address,
                     const net::IPEndPoint& remote_address) override;
  void SendComplete(const network::P2PSendPacketMetrics& send_metrics) override;
  void IncomingTcpConnection(
      const net::IPEndPoint& socket_address,
      network::mojom::P2PSocketPtr socket,
      network::mojom::P2PSocketClientRequest client_request) override;
  void DataReceived(const net::IPEndPoint& socket_address,
                    const std::vector<int8_t>& data,
                    base::TimeTicks timestamp) override;

  void OnConnectionError();

  P2PSocketDispatcher* dispatcher_;
  base::ThreadChecker thread_checker_;
  int socket_id_;
  P2PSocketClientDelegate* delegate_;
  State state_;
  const net::NetworkTrafficAnnotationTag traffic_annotation_;

  // These two fields are used to identify packets for tracing.
  uint32_t random_socket_id_;
  uint32_t next_packet_id_;

  network::mojom::P2PSocketPtr socket_;
  mojo::Binding<network::mojom::P2PSocketClient> binding_;

  DISALLOW_COPY_AND_ASSIGN(P2PSocketClientImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_P2P_SOCKET_CLIENT_IMPL_H_
