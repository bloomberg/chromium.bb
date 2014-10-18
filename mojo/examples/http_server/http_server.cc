// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <arpa/inet.h>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "mojo/common/handle_watcher.h"
#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/cpp/application/application_runner.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/services/public/interfaces/network/network_service.mojom.h"

namespace mojo {
namespace examples {

// The canned response that this server replies to each request with.
const char kResponse[] =
    "HTTP/1.0 200 OK\n"
    "Content-Type: text/html; charset=utf-8\n"
    "\n"
    "Hello, world\n";

// Represents one connection to a client. This connection will manage its own
// lifetime and will delete itself when the connection is closed.
class Connection {
 public:
  Connection(TCPConnectedSocketPtr conn,
             ScopedDataPipeProducerHandle sender,
             ScopedDataPipeConsumerHandle receiver)
      : connection_(conn.Pass()),
        sender_(sender.Pass()),
        receiver_(receiver.Pass()),
        weak_ptr_factory_(this),
        response_(kResponse),
        response_offset_(0) {
    WriteMore();
  }

  ~Connection() {
  }

 private:
  void OnSenderReady(MojoResult result) {
    WriteMore();
  }

  void WriteMore() {
    uint32_t num_bytes =
        static_cast<uint32_t>(response_.size() - response_offset_);
    // TODO(brettw) write chunks (ideally capped at some small size to
    // illustrate how it works even in the presence of our relatively small
    // reply) rather than specifying ALL_OR_NONE. See below.
    MojoResult result = WriteDataRaw(
        sender_.get(), &response_[response_offset_], &num_bytes,
        MOJO_WRITE_DATA_FLAG_ALL_OR_NONE);
    if (result == MOJO_RESULT_SHOULD_WAIT) {
      num_bytes = 0;
    } else if (result != MOJO_RESULT_OK) {
      printf("Error writing to pipe.\n");
      delete this;
      return;
    }

    response_offset_ += num_bytes;

    if (response_offset_ == response_.size()) {
      // Connection complete.
      delete this;
      return;
    }

    // Wait for sender to be writable.
    // TODO(brettw) we won't actually get here since we ALL_OR_NONE was
    // specified above. This call won't work because the message loop is not a
    // Chromium message loop. Instead, we probably need to use
    // Environment::GetDefaultAsyncWaiter to wait on the handles and remove the
    // ALL_OR_NONE flag.
    sender_watcher_.Start(
        sender_.get(),
        MOJO_HANDLE_SIGNAL_WRITABLE, MOJO_DEADLINE_INDEFINITE,
        base::Bind(&Connection::OnSenderReady, weak_ptr_factory_.GetWeakPtr()));
  }

  TCPConnectedSocketPtr connection_;
  ScopedDataPipeProducerHandle sender_;
  ScopedDataPipeConsumerHandle receiver_;

  base::WeakPtrFactory<Connection> weak_ptr_factory_;
  common::HandleWatcher sender_watcher_;

  std::string response_;
  size_t response_offset_;
};

class HttpServerApp : public ApplicationDelegate {
 public:
  virtual void Initialize(ApplicationImpl* app) override {
    app->ConnectToService("mojo:network_service", &network_service_);
    Start();
  }

 private:
  void OnSocketBound(NetworkErrorPtr err, NetAddressPtr bound_address) {
    if (err->code != 0) {
      printf("Bound err = %d\n", err->code);
      return;
    }

    printf("Got address %d.%d.%d.%d:%d\n",
           (int)bound_address->ipv4->addr[0],
           (int)bound_address->ipv4->addr[1],
           (int)bound_address->ipv4->addr[2],
           (int)bound_address->ipv4->addr[3],
           (int)bound_address->ipv4->port);
  }

  void OnSocketListening(NetworkErrorPtr err) {
    if (err->code != 0) {
      printf("Listen err = %d\n", err->code);
      return;
    }
    printf("Waiting for incoming connections...\n");
  }

  void OnConnectionAccepted(NetworkErrorPtr err, NetAddressPtr remote_address) {
    if (err->code != 0) {
      printf("Accepted socket error = %d\n", err->code);
      return;
    }

    new Connection(pending_connected_socket_.Pass(),
                   pending_send_handle_.Pass(),
                   pending_receive_handle_.Pass());

    // Ready for another connection.
    WaitForNextConnection();
  }

  void WaitForNextConnection() {
    // Need two pipes (one for each direction).
    ScopedDataPipeConsumerHandle send_consumer_handle;
    MojoResult result = CreateDataPipe(
        nullptr, &pending_send_handle_, &send_consumer_handle);
    assert(result == MOJO_RESULT_OK);

    ScopedDataPipeProducerHandle receive_producer_handle;
    result = CreateDataPipe(
        nullptr, &receive_producer_handle, &pending_receive_handle_);
    assert(result == MOJO_RESULT_OK);
    MOJO_ALLOW_UNUSED_LOCAL(result);

    server_socket_->Accept(send_consumer_handle.Pass(),
                           receive_producer_handle.Pass(),
                           GetProxy(&pending_connected_socket_),
                           base::Bind(&HttpServerApp::OnConnectionAccepted,
                                      base::Unretained(this)));
  }

  void Start() {
    NetAddressPtr net_address(NetAddress::New());
    net_address->family = NET_ADDRESS_FAMILY_IPV4;
    net_address->ipv4 = NetAddressIPv4::New();
    net_address->ipv4->addr.resize(4);
    net_address->ipv4->addr[0] = 127;
    net_address->ipv4->addr[1] = 0;
    net_address->ipv4->addr[2] = 0;
    net_address->ipv4->addr[3] = 1;
    net_address->ipv4->port = 0;

    // Note that we can start using the proxies right away even thought the
    // callbacks have not been called yet. If a previous step fails, they'll
    // all fail.
    network_service_->CreateTCPBoundSocket(
        net_address.Pass(),
        GetProxy(&bound_socket_),
        base::Bind(&HttpServerApp::OnSocketBound, base::Unretained(this)));
    bound_socket_->StartListening(GetProxy(
        &server_socket_),
        base::Bind(&HttpServerApp::OnSocketListening, base::Unretained(this)));
    WaitForNextConnection();
  }

  NetworkServicePtr network_service_;
  TCPBoundSocketPtr bound_socket_;
  TCPServerSocketPtr server_socket_;

  ScopedDataPipeProducerHandle pending_send_handle_;
  ScopedDataPipeConsumerHandle pending_receive_handle_;
  TCPConnectedSocketPtr pending_connected_socket_;
};

}  // namespace examples
}  // namespace mojo

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::ApplicationRunner runner(new mojo::examples::HttpServerApp);
  return runner.Run(shell_handle);
}
