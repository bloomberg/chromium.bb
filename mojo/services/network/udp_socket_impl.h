// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_NETWORK_UDP_SOCKET_IMPL_H_
#define MOJO_SERVICES_NETWORK_UDP_SOCKET_IMPL_H_

#include <deque>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "mojo/public/cpp/bindings/interface_impl.h"
#include "mojo/services/public/interfaces/network/udp_socket.mojom.h"
#include "net/base/ip_endpoint.h"
#include "net/udp/udp_server_socket.h"

namespace net {
class IOBuffer;
class IOBufferWithSize;
}

namespace mojo {

class UDPSocketImpl : public InterfaceImpl<UDPSocket> {
 public:
  UDPSocketImpl();
  ~UDPSocketImpl() override;

  // UDPSocket implementation.
  void AllowAddressReuse(
      const Callback<void(NetworkErrorPtr)>& callback) override;

  void Bind(
      NetAddressPtr addr,
      const Callback<void(NetworkErrorPtr, NetAddressPtr)>& callback) override;

  void SetSendBufferSize(
      uint32_t size,
      const Callback<void(NetworkErrorPtr)>& callback) override;

  void SetReceiveBufferSize(
      uint32_t size,
      const Callback<void(NetworkErrorPtr)>& callback) override;

  void NegotiateMaxPendingSendRequests(
      uint32_t requested_size,
      const Callback<void(uint32_t)>& callback) override;

  void ReceiveMore(uint32_t datagram_number) override;

  void SendTo(NetAddressPtr dest_addr,
              Array<uint8_t> data,
              const Callback<void(NetworkErrorPtr)>& callback) override;

 private:
  struct PendingSendRequest {
    PendingSendRequest();
    ~PendingSendRequest();

    NetAddressPtr addr;
    Array<uint8_t> data;
    Callback<void(NetworkErrorPtr)> callback;
  };

  void DoRecvFrom();
  void DoSendTo(NetAddressPtr addr,
                Array<uint8_t> data,
                const Callback<void(NetworkErrorPtr)>& callback);

  void OnRecvFromCompleted(int net_result);
  void OnSendToCompleted(const Callback<void(NetworkErrorPtr)>& callback,
                         int net_result);

  net::UDPServerSocket socket_;

  bool bound_;

  // Non-NULL when there is a pending RecvFrom operation on |socket_|.
  scoped_refptr<net::IOBuffer> recvfrom_buffer_;
  // Non-NULL when there is a pending SendTo operation on |socket_|.
  scoped_refptr<net::IOBufferWithSize> sendto_buffer_;

  net::IPEndPoint recvfrom_address_;

  // How many more packets the client side expects to receive.
  size_t remaining_recv_slots_;

  // The queue owns the PendingSendRequest instances.
  std::deque<PendingSendRequest*> pending_send_requests_;
  // The maximum size of the |pending_send_requests_| queue.
  size_t max_pending_send_requests_;

  DISALLOW_COPY_AND_ASSIGN(UDPSocketImpl);
};

}  // namespace mojo

#endif  // MOJO_SERVICES_NETWORK_UDP_SOCKET_IMPL_H_
