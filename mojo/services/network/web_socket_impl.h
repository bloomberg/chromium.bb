// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_NETWORK_WEB_SOCKET_IMPL_H_
#define MOJO_SERVICES_NETWORK_WEB_SOCKET_IMPL_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/public/cpp/bindings/interface_impl.h"
#include "mojo/services/public/interfaces/network/web_socket.mojom.h"

namespace net {
class WebSocketChannel;
}  // namespace net

namespace mojo {
class NetworkContext;
class WebSocketReadQueue;

// Forms a bridge between the WebSocket mojo interface and the net::WebSocket
// implementation.
class WebSocketImpl : public InterfaceImpl<WebSocket> {
 public:
  explicit WebSocketImpl(NetworkContext* context);
  virtual ~WebSocketImpl();

 private:
  // WebSocket methods:
  virtual void Connect(const String& url,
                       Array<String> protocols,
                       const String& origin,
                       ScopedDataPipeConsumerHandle send_stream,
                       WebSocketClientPtr client) OVERRIDE;
  virtual void Send(bool fin,
                    WebSocket::MessageType type,
                    uint32_t num_bytes) OVERRIDE;
  virtual void FlowControl(int64_t quota) OVERRIDE;
  virtual void Close(uint16_t code, const String& reason) OVERRIDE;

  // Called with the data to send once it has been read from |send_stream_|.
  void DidReadFromSendStream(bool fin,
                             WebSocket::MessageType type,
                             uint32_t num_bytes,
                             const char* data);

  // The channel we use to send events to the network.
  scoped_ptr<net::WebSocketChannel> channel_;
  ScopedDataPipeConsumerHandle send_stream_;
  scoped_ptr<WebSocketReadQueue> read_queue_;
  NetworkContext* context_;
};

}  // namespace mojo

#endif  // MOJO_SERVICES_NETWORK_WEB_SOCKET_IMPL_H_
