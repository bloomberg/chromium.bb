// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_PUBLIC_CPP_NETWORK_UDP_SOCKET_WRAPPER_H_
#define MOJO_SERVICES_PUBLIC_CPP_NETWORK_UDP_SOCKET_WRAPPER_H_

#include <queue>

#include "mojo/services/public/interfaces/network/udp_socket.mojom.h"

namespace mojo {

// This class is a wrapper around the UDPSocket interface. It provides local
// cache for received datagrams as well as for (excessive) send requests:
// - You call ReceiveFrom() to retrieve one datagram. If there is already cached
//   data, the operation completes synchronously.
// - You don't need to worry about the max-pending-send-requests restriction
//   imposed by the service side. If you make many SendTo() calls in a short
//   period of time, it caches excessive requests and sends them later.
class UDPSocketWrapper : public UDPSocketClient {
 public:
  typedef Callback<void(NetworkErrorPtr, NetAddressPtr, Array<uint8_t>)>
      ReceiveCallback;
  typedef Callback<void(NetworkErrorPtr)> ErrorCallback;

  explicit UDPSocketWrapper(UDPSocketPtr socket);

  // |receive_queue_slots| determines the size (in datagrams) of the local
  // receive queue, which caches incoming datagrams.
  // |requested_max_pending_sends| is used to call
  // NegotiateMaxPendingSendRequests() on |socket|.
  // The two numbers should be greater than 0. If you would like to use default
  // values, please use the other constructor.
  UDPSocketWrapper(UDPSocketPtr socket,
                   uint32_t receive_queue_slots,
                   uint32_t requested_max_pending_sends);

  ~UDPSocketWrapper() override;

  void AllowAddressReuse(const ErrorCallback& callback);

  void Bind(NetAddressPtr addr,
            const Callback<void(NetworkErrorPtr, NetAddressPtr)>& callback);

  void SetSendBufferSize(uint32_t size, const ErrorCallback& callback);

  void SetReceiveBufferSize(uint32_t size, const ErrorCallback& callback);

  // If there are already incoming datagrams cached locally, this method runs
  // |callback| before it returns, and the return value is set to true.
  // Otherwise, the return value is set to false and the callback will be run
  // asynchronously.
  bool ReceiveFrom(const ReceiveCallback& callback);

  // This method is aware of the max pending send requests allowed by the
  // service, and caches send requests locally if necessary.
  void SendTo(NetAddressPtr dest_addr,
              Array<uint8_t> data,
              const ErrorCallback& callback);

 private:
  class NegotiateCallbackHandler : public Callback<void(uint32_t)>::Runnable {
   public:
    explicit NegotiateCallbackHandler(UDPSocketWrapper* delegate);
    ~NegotiateCallbackHandler() override;

    // Callback<void(uint32_t)>::Runnable implementation:
    void Run(uint32_t actual_size) const override;

   private:
    // Because this callback is passed to a method of |socket_|, and |socket_|
    // is owned by |delegate_|, it should be safe to assume that |delegate_| is
    // valid if/when Run() is called.
    UDPSocketWrapper* delegate_;
  };

  class SendCallbackHandler : public ErrorCallback::Runnable {
   public:
    explicit SendCallbackHandler(UDPSocketWrapper* delegate,
                                 const ErrorCallback& forward_callback);
    ~SendCallbackHandler() override;

    // ErrorCallback::Runnable implementation:
    void Run(NetworkErrorPtr result) const override;

   private:
    // Because this callback is passed to a method of |socket_|, and |socket_|
    // is owned by |delegate_|, it should be safe to assume that |delegate_| is
    // valid if/when Run() is called.
    UDPSocketWrapper* delegate_;
    ErrorCallback forward_callback_;
  };

  struct ReceivedData {
    ReceivedData();
    ~ReceivedData();

    NetworkErrorPtr result;
    NetAddressPtr src_addr;
    Array<uint8_t> data;
  };

  struct SendRequest {
    SendRequest();
    ~SendRequest();

    NetAddressPtr dest_addr;
    Array<uint8_t> data;
    ErrorCallback callback;
  };

  // UDPSocketClient implementation:
  void OnReceived(NetworkErrorPtr result,
                  NetAddressPtr src_addr,
                  Array<uint8_t> data) override;

  void Initialize(uint32_t requested_max_pending_sends);
  void OnNegotiateMaxPendingSendRequestsCompleted(uint32_t actual_size);

  void OnSendToCompleted(NetworkErrorPtr result,
                         const ErrorCallback& forward_callback);

  // Returns true if a send request in |send_requests_| has been processed.
  bool ProcessNextSendRequest();

  UDPSocketPtr socket_;

  uint32_t max_receive_queue_size_;

  // Owns all the objects that its elements point to.
  std::queue<ReceivedData*> receive_queue_;

  std::queue<ReceiveCallback> receive_requests_;

  uint32_t max_pending_sends_;
  uint32_t current_pending_sends_;

  // Owns all the objects that its elements point to.
  std::queue<SendRequest*> send_requests_;
};

}  // namespace mojo

#endif  // MOJO_SERVICES_PUBLIC_CPP_NETWORK_UDP_SOCKET_WRAPPER_H_
