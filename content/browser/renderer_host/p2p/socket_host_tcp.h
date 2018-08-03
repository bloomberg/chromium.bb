// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_HOST_TCP_H_
#define CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_HOST_TCP_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/browser/renderer_host/p2p/socket_host.h"
#include "net/base/completion_callback.h"
#include "net/base/ip_endpoint.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/p2p_socket_type.h"

namespace network {
class ProxyResolvingClientSocketFactory;
}  // namespace network

namespace net {
class DrainableIOBuffer;
class GrowableIOBuffer;
class StreamSocket;
class URLRequestContextGetter;
}  // namespace net

namespace content {

class CONTENT_EXPORT P2PSocketHostTcpBase : public P2PSocketHost {
 public:
  P2PSocketHostTcpBase(P2PSocketDispatcherHost* socket_dispatcher_host,
                       network::mojom::P2PSocketClientPtr client,
                       network::mojom::P2PSocketRequest socket,
                       network::P2PSocketType type,
                       net::URLRequestContextGetter* url_context,
                       network::ProxyResolvingClientSocketFactory*
                           proxy_resolving_socket_factory);
  ~P2PSocketHostTcpBase() override;

  bool InitAccepted(const net::IPEndPoint& remote_address,
                    std::unique_ptr<net::StreamSocket> socket);

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

 protected:
  struct SendBuffer {
    SendBuffer();
    SendBuffer(int32_t packet_id,
               scoped_refptr<net::DrainableIOBuffer> buffer,
               const net::NetworkTrafficAnnotationTag traffic_annotation);
    SendBuffer(const SendBuffer& rhs);
    ~SendBuffer();

    int32_t rtc_packet_id;
    scoped_refptr<net::DrainableIOBuffer> buffer;
    net::MutableNetworkTrafficAnnotationTag traffic_annotation;
  };

  // Derived classes will provide the implementation.
  virtual int ProcessInput(char* input, int input_len) = 0;
  virtual void DoSend(
      const net::IPEndPoint& to,
      const std::vector<int8_t>& data,
      const rtc::PacketOptions& options,
      const net::NetworkTrafficAnnotationTag traffic_annotation) = 0;

  void WriteOrQueue(SendBuffer& send_buffer);
  void OnPacket(const std::vector<int8_t>& data);
  void OnError();

 private:
  friend class P2PSocketHostTcpTestBase;
  friend class P2PSocketHostTcpServerTest;

  void DidCompleteRead(int result);
  void DoRead();

  void DoWrite();
  void HandleWriteResult(int result);

  // Callbacks for Connect(), Read() and Write().
  void OnConnected(int result);
  void OnRead(int result);
  void OnWritten(int result);

  // Helper method to send socket create message and start read.
  void OnOpen();
  bool DoSendSocketCreateMsg();

  network::P2PHostAndIPEndPoint remote_address_;

  std::unique_ptr<net::StreamSocket> socket_;
  scoped_refptr<net::GrowableIOBuffer> read_buffer_;
  base::queue<SendBuffer> write_queue_;
  SendBuffer write_buffer_;

  bool write_pending_;

  bool connected_;
  network::P2PSocketType type_;
  scoped_refptr<net::URLRequestContextGetter> url_context_;
  network::ProxyResolvingClientSocketFactory* proxy_resolving_socket_factory_;

  DISALLOW_COPY_AND_ASSIGN(P2PSocketHostTcpBase);
};

class CONTENT_EXPORT P2PSocketHostTcp : public P2PSocketHostTcpBase {
 public:
  P2PSocketHostTcp(P2PSocketDispatcherHost* socket_dispatcher_host,
                   network::mojom::P2PSocketClientPtr client,
                   network::mojom::P2PSocketRequest socket,
                   network::P2PSocketType type,
                   net::URLRequestContextGetter* url_context,
                   network::ProxyResolvingClientSocketFactory*
                       proxy_resolving_socket_factory);

  ~P2PSocketHostTcp() override;

 protected:
  int ProcessInput(char* input, int input_len) override;
  void DoSend(
      const net::IPEndPoint& to,
      const std::vector<int8_t>& data,
      const rtc::PacketOptions& options,
      const net::NetworkTrafficAnnotationTag traffic_annotation) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(P2PSocketHostTcp);
};

// P2PSocketHostStunTcp class provides the framing of STUN messages when used
// with TURN. These messages will not have length at front of the packet and
// are padded to multiple of 4 bytes.
// Formatting of messages is defined in RFC5766.
class CONTENT_EXPORT P2PSocketHostStunTcp : public P2PSocketHostTcpBase {
 public:
  P2PSocketHostStunTcp(P2PSocketDispatcherHost* socket_dispatcher_host,
                       network::mojom::P2PSocketClientPtr client,
                       network::mojom::P2PSocketRequest socket,
                       network::P2PSocketType type,
                       net::URLRequestContextGetter* url_context,
                       network::ProxyResolvingClientSocketFactory*
                           proxy_resolving_socket_factory);

  ~P2PSocketHostStunTcp() override;

 protected:
  int ProcessInput(char* input, int input_len) override;
  void DoSend(
      const net::IPEndPoint& to,
      const std::vector<int8_t>& data,
      const rtc::PacketOptions& options,
      const net::NetworkTrafficAnnotationTag traffic_annotation) override;

 private:
  int GetExpectedPacketSize(const int8_t* data, int len, int* pad_bytes);

  DISALLOW_COPY_AND_ASSIGN(P2PSocketHostStunTcp);
};


}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_HOST_TCP_H_
