// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_HOST_TCP_SERVER_H_
#define CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_HOST_TCP_SERVER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/browser/renderer_host/p2p/socket_host.h"
#include "content/common/content_export.h"
#include "ipc/ipc_sender.h"
#include "net/base/completion_repeating_callback.h"
#include "net/socket/tcp_server_socket.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/p2p_socket_type.h"

namespace net {
class StreamSocket;
}  // namespace net

namespace content {
class P2PSocketHostTcpBase;

class CONTENT_EXPORT P2PSocketHostTcpServer : public P2PSocketHost {
 public:
  P2PSocketHostTcpServer(P2PSocketDispatcherHost* socket_dispatcher_host,
                         network::mojom::P2PSocketClientPtr client,
                         network::mojom::P2PSocketRequest socket,
                         network::P2PSocketType client_type);
  ~P2PSocketHostTcpServer() override;

  // P2PSocketHost overrides.
  bool Init(const net::IPEndPoint& local_address,
            uint16_t min_port,
            uint16_t max_port,
            const network::P2PHostAndIPEndPoint& remote_address) override;

  // network::mojom::P2PSocket implementation:
  void AcceptIncomingTcpConnection(
      const net::IPEndPoint& remote_address,
      network::mojom::P2PSocketClientPtr client,
      network::mojom::P2PSocketRequest request) override;
  void Send(const std::vector<int8_t>& data,
            const network::P2PPacketInfo& packet_info,
            const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;
  void SetOption(network::P2PSocketOption option, int32_t value) override;

 private:
  friend class P2PSocketHostTcpServerTest;

  std::unique_ptr<P2PSocketHostTcpBase> AcceptIncomingTcpConnectionInternal(
      const net::IPEndPoint& remote_address,
      network::mojom::P2PSocketClientPtr client,
      network::mojom::P2PSocketRequest request);

  void OnError();

  void DoAccept();
  void HandleAcceptResult(int result);

  // Callback for Accept().
  void OnAccepted(int result);

  const network::P2PSocketType client_type_;
  std::unique_ptr<net::ServerSocket> socket_;
  net::IPEndPoint local_address_;

  std::unique_ptr<net::StreamSocket> accept_socket_;
  std::map<net::IPEndPoint, std::unique_ptr<net::StreamSocket>>
      accepted_sockets_;

  const net::CompletionRepeatingCallback accept_callback_;

  DISALLOW_COPY_AND_ASSIGN(P2PSocketHostTcpServer);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_HOST_TCP_SERVER_H_
