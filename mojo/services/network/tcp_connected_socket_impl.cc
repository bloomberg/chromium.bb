// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/network/tcp_connected_socket_impl.h"

#include "base/message_loop/message_loop.h"
#include "mojo/services/network/net_adapters.h"
#include "net/base/net_errors.h"

namespace mojo {

TCPConnectedSocketImpl::TCPConnectedSocketImpl(
    scoped_ptr<net::TCPSocket> socket,
    ScopedDataPipeConsumerHandle send_stream,
    ScopedDataPipeProducerHandle receive_stream)
    : socket_(socket.Pass()),
      send_stream_(send_stream.Pass()),
      receive_stream_(receive_stream.Pass()),
      weak_ptr_factory_(this) {
  // Queue up async communication.
  ReceiveMore();
  SendMore();
}

TCPConnectedSocketImpl::~TCPConnectedSocketImpl() {
}

void TCPConnectedSocketImpl::ReceiveMore() {
  DCHECK(!pending_receive_.get());

  uint32_t num_bytes;
  MojoResult result = NetToMojoPendingBuffer::BeginWrite(
      &receive_stream_, &pending_receive_, &num_bytes);
  if (result == MOJO_RESULT_SHOULD_WAIT) {
    // The pipe is full. We need to wait for it to have more space.
    receive_handle_watcher_.Start(
        receive_stream_.get(),
        MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
        MOJO_DEADLINE_INDEFINITE,
        base::Bind(&TCPConnectedSocketImpl::OnReceiveStreamReady,
                   weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  if (result == MOJO_RESULT_FAILED_PRECONDITION) {
    // It's valid that the user of this class consumed the data they care about
    // and closed their data pipe handles after writing data. This class should
    // still write out all the data.
    ShutdownReceive();
    // TODO(johnmccutchan): Notify socket direction is closed along with
    // net_result and mojo_result.
    return;
  }

  if (result != MOJO_RESULT_OK) {
    // The receive stream is in a bad state.
    ShutdownReceive();
    // TODO(johnmccutchan): Notify socket direction is closed along with
    // net_result and mojo_result.
    return;
  }

  // Mojo is ready for the receive.
  CHECK_GT(static_cast<uint32_t>(std::numeric_limits<int>::max()), num_bytes);
  scoped_refptr<net::IOBuffer> buf(
      new NetToMojoIOBuffer(pending_receive_.get()));
  int read_result = socket_->Read(
      buf.get(), static_cast<int>(num_bytes),
      base::Bind(&TCPConnectedSocketImpl::DidReceive, base::Unretained(this),
                 false));
  if (read_result == net::ERR_IO_PENDING) {
    // Pending I/O, wait for result in DidReceive().
  } else if (read_result > 0) {
    // Synchronous data ready.
    DidReceive(true, read_result);
  } else {
    // read_result == 0 indicates EOF.
    // read_result < 0 indicates error.
    ShutdownReceive();
    // TODO(johnmccutchan): Notify socket direction is closed along with
    // net_result and mojo_result.
  }
}

void TCPConnectedSocketImpl::OnReceiveStreamReady(MojoResult result) {
  if (result != MOJO_RESULT_OK) {
    ShutdownReceive();
    // TODO(johnmccutchan): Notify socket direction is closed along with
    // net_result and mojo_result.
    return;
  }
  ReceiveMore();
}

void TCPConnectedSocketImpl::DidReceive(bool completed_synchronously,
                                        int result) {
  if (result < 0) {
    // Error.
    ShutdownReceive();
    // TODO(johnmccutchan): Notify socket direction is closed along with
    // net_result and mojo_result.
    return;
  }

  receive_stream_ = pending_receive_->Complete(result);
  pending_receive_ = nullptr;

  // Schedule more reading.
  if (completed_synchronously) {
    // Don't recursively call ReceiveMore if this is a sync read.
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&TCPConnectedSocketImpl::ReceiveMore,
                   weak_ptr_factory_.GetWeakPtr()));
  } else {
    ReceiveMore();
  }
}

void TCPConnectedSocketImpl::ShutdownReceive() {
  pending_receive_ = nullptr;
  receive_stream_.reset();
}

void TCPConnectedSocketImpl::SendMore() {
  uint32_t num_bytes = 0;
  MojoResult result = MojoToNetPendingBuffer::BeginRead(
      &send_stream_, &pending_send_, &num_bytes);
  if (result == MOJO_RESULT_SHOULD_WAIT) {
    // Data not ready, wait for it.
    send_handle_watcher_.Start(
        send_stream_.get(),
        MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
        MOJO_DEADLINE_INDEFINITE,
        base::Bind(&TCPConnectedSocketImpl::OnSendStreamReady,
                   weak_ptr_factory_.GetWeakPtr()));
    return;
  } else if (result != MOJO_RESULT_OK) {
    ShutdownSend();
    // TODO(johnmccutchan): Notify socket direction is closed along with
    // net_result and mojo_result.
    return;
  }

  // Got a buffer from Mojo, give it to the socket. Note that the sockets may
  // do partial writes.
  scoped_refptr<net::IOBuffer> buf(new MojoToNetIOBuffer(pending_send_.get()));
  int write_result = socket_->Write(
      buf.get(), static_cast<int>(num_bytes),
      base::Bind(&TCPConnectedSocketImpl::DidSend, base::Unretained(this),
                 false));
  if (write_result == net::ERR_IO_PENDING) {
    // Pending I/O, wait for result in DidSend().
  } else if (write_result >= 0) {
    // Synchronous data consumed.
    DidSend(true, write_result);
  } else {
    // write_result < 0 indicates error.
    ShutdownSend();
    // TODO(johnmccutchan): Notify socket direction is closed along with
    // net_result and mojo_result.
  }
}

void TCPConnectedSocketImpl::OnSendStreamReady(MojoResult result) {
  if (result != MOJO_RESULT_OK) {
    ShutdownSend();
    // TODO(johnmccutchan): Notify socket direction is closed along with
    // net_result and mojo_result.
    return;
  }
  SendMore();
}

void TCPConnectedSocketImpl::DidSend(bool completed_synchronously,
                                     int result) {
  if (result < 0) {
    ShutdownSend();
    // TODO(johnmccutchan): Notify socket direction is closed along with
    // net_result and mojo_result.
    return;
  }

  // Take back ownership of the stream and free the IOBuffer.
  send_stream_ = pending_send_->Complete(result);
  pending_send_ = nullptr;

  // Schedule more writing.
  if (completed_synchronously) {
    // Don't recursively call SendMore if this is a sync read.
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&TCPConnectedSocketImpl::SendMore,
                   weak_ptr_factory_.GetWeakPtr()));
  } else {
    SendMore();
  }
}

void TCPConnectedSocketImpl::ShutdownSend() {
  pending_send_ = nullptr;
  send_stream_.reset();
}

}  // namespace mojo
