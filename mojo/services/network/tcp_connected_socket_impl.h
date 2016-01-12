// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_NETWORK_TCP_CONNECTED_SOCKET_H_
#define MOJO_SERVICES_NETWORK_TCP_CONNECTED_SOCKET_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "mojo/message_pump/handle_watcher.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/services/network/public/interfaces/tcp_connected_socket.mojom.h"
#include "mojo/shell/public/cpp/app_lifetime_helper.h"
#include "net/socket/tcp_socket.h"

namespace mojo {

class MojoToNetPendingBuffer;
class NetToMojoPendingBuffer;

class TCPConnectedSocketImpl : public TCPConnectedSocket {
 public:
  TCPConnectedSocketImpl(scoped_ptr<net::TCPSocket> socket,
                         ScopedDataPipeConsumerHandle send_stream,
                         ScopedDataPipeProducerHandle receive_stream,
                         InterfaceRequest<TCPConnectedSocket> request,
                         scoped_ptr<mojo::AppRefCount> app_refcount);
  ~TCPConnectedSocketImpl() override;

 private:
  void OnConnectionError();

  // "Receiving" in this context means reading from TCPSocket and writing to
  // the Mojo receive_stream.
  void ReceiveMore();
  void OnReceiveStreamReady(MojoResult result);
  void DidReceive(bool completed_synchronously, int result);
  void ShutdownReceive();
  void ListenForReceivePeerClosed();
  void OnReceiveDataPipeClosed(MojoResult result);

  // "Writing" is reading from the Mojo send_stream and writing to the
  // TCPSocket.
  void SendMore();
  void OnSendStreamReady(MojoResult result);
  void DidSend(bool completed_asynchronously, int result);
  void ShutdownSend();
  void ListenForSendPeerClosed();
  void OnSendDataPipeClosed(MojoResult result);

  void DeleteIfNeeded();

  scoped_ptr<net::TCPSocket> socket_;

  // The *stream handles will be null while there is an in-progress transation
  // between net and mojo. During this time, the handle will be owned by the
  // *PendingBuffer.

  // For reading from the network and writing to Mojo pipe.
  ScopedDataPipeConsumerHandle send_stream_;
  scoped_refptr<NetToMojoPendingBuffer> pending_receive_;
  common::HandleWatcher receive_handle_watcher_;

  // For reading from the Mojo pipe and writing to the network.
  ScopedDataPipeProducerHandle receive_stream_;
  scoped_refptr<MojoToNetPendingBuffer> pending_send_;
  common::HandleWatcher send_handle_watcher_;

  // To bind to the message pipe.
  Binding<TCPConnectedSocket> binding_;

  scoped_ptr<mojo::AppRefCount> app_refcount_;

  base::WeakPtrFactory<TCPConnectedSocketImpl> weak_ptr_factory_;
};

}  // namespace mojo

#endif  // MOJO_SERVICES_NETWORK_TCP_CONNECTED_SOCKET_H_
