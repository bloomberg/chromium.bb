// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper_tcp_socket.h"

#include <string.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "content/browser/renderer_host/pepper_message_filter.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/cert_verifier.h"
#include "net/base/host_port_pair.h"
#include "net/base/host_resolver.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/single_request_host_resolver.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/tcp_client_socket.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/private/net_address_private_impl.h"
#include "ppapi/shared_impl/private/tcp_socket_private_impl.h"

using content::BrowserThread;
using ppapi::NetAddressPrivateImpl;

PepperTCPSocket::PepperTCPSocket(
    PepperMessageFilter* manager,
    int32 routing_id,
    uint32 plugin_dispatcher_id,
    uint32 socket_id)
    : manager_(manager),
      routing_id_(routing_id),
      plugin_dispatcher_id_(plugin_dispatcher_id),
      socket_id_(socket_id),
      connection_state_(BEFORE_CONNECT),
      end_of_file_reached_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(connect_callback_(
          this, &PepperTCPSocket::OnConnectCompleted)),
      ALLOW_THIS_IN_INITIALIZER_LIST(ssl_handshake_callback_(
          this, &PepperTCPSocket::OnSSLHandshakeCompleted)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          read_callback_(this, &PepperTCPSocket::OnReadCompleted)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          write_callback_(this, &PepperTCPSocket::OnWriteCompleted)) {
  DCHECK(manager);
}

PepperTCPSocket::~PepperTCPSocket() {
  // Make sure no further callbacks from socket_.
  if (socket_.get())
    socket_->Disconnect();
}

void PepperTCPSocket::Connect(const std::string& host, uint16_t port) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (connection_state_ != BEFORE_CONNECT) {
    SendConnectACKError();
    return;
  }

  connection_state_ = CONNECT_IN_PROGRESS;
  net::HostResolver::RequestInfo request_info(net::HostPortPair(host, port));
  resolver_.reset(new net::SingleRequestHostResolver(
      manager_->GetHostResolver()));
  int result = resolver_->Resolve(
      request_info, &address_list_,
      base::Bind(&PepperTCPSocket::OnResolveCompleted, base::Unretained(this)),
      net::BoundNetLog());
  if (result != net::ERR_IO_PENDING)
    OnResolveCompleted(result);
}

void PepperTCPSocket::ConnectWithNetAddress(
    const PP_NetAddress_Private& net_addr) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (connection_state_ != BEFORE_CONNECT ||
      !NetAddressPrivateImpl::NetAddressToAddressList(net_addr,
                                                      &address_list_)) {
    SendConnectACKError();
    return;
  }

  connection_state_ = CONNECT_IN_PROGRESS;
  StartConnect(address_list_);
}

void PepperTCPSocket::SSLHandshake(const std::string& server_name,
                                   uint16_t server_port) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Allow to do SSL handshake only if currently the socket has been connected
  // and there isn't pending read or write.
  // IsConnected() includes the state that SSL handshake has been finished and
  // therefore isn't suitable here.
  if (connection_state_ != CONNECTED || read_buffer_.get() ||
      write_buffer_.get()) {
    SendSSLHandshakeACK(false);
    return;
  }

  connection_state_ = SSL_HANDSHAKE_IN_PROGRESS;
  net::ClientSocketHandle* handle = new net::ClientSocketHandle();
  handle->set_socket(socket_.release());
  net::ClientSocketFactory* factory =
      net::ClientSocketFactory::GetDefaultFactory();
  net::HostPortPair host_port_pair(server_name, server_port);
  net::SSLClientSocketContext ssl_context;
  ssl_context.cert_verifier = manager_->GetCertVerifier();
  socket_.reset(factory->CreateSSLClientSocket(
      handle, host_port_pair, manager_->ssl_config(), NULL, ssl_context));
  if (!socket_.get()) {
    LOG(WARNING) << "Failed to create an SSL client socket.";
    OnSSLHandshakeCompleted(net::ERR_UNEXPECTED);
    return;
  }

  int result = socket_->Connect(&ssl_handshake_callback_);
  if (result != net::ERR_IO_PENDING)
    OnSSLHandshakeCompleted(result);
}

void PepperTCPSocket::Read(int32 bytes_to_read) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (!IsConnected() || end_of_file_reached_ || read_buffer_.get() ||
      bytes_to_read <= 0) {
    SendReadACKError();
    return;
  }

  if (bytes_to_read > ppapi::TCPSocketPrivateImpl::kMaxReadSize) {
    NOTREACHED();
    bytes_to_read = ppapi::TCPSocketPrivateImpl::kMaxReadSize;
  }

  read_buffer_ = new net::IOBuffer(bytes_to_read);
  int result = socket_->Read(read_buffer_, bytes_to_read, &read_callback_);
  if (result != net::ERR_IO_PENDING)
    OnReadCompleted(result);
}

void PepperTCPSocket::Write(const std::string& data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (!IsConnected() || write_buffer_.get() || data.empty()) {
    SendWriteACKError();
    return;
  }

  int data_size = data.size();
  if (data_size > ppapi::TCPSocketPrivateImpl::kMaxWriteSize) {
    NOTREACHED();
    data_size = ppapi::TCPSocketPrivateImpl::kMaxWriteSize;
  }

  write_buffer_ = new net::IOBuffer(data_size);
  memcpy(write_buffer_->data(), data.c_str(), data_size);
  int result = socket_->Write(write_buffer_, data.size(), &write_callback_);
  if (result != net::ERR_IO_PENDING)
    OnWriteCompleted(result);
}

void PepperTCPSocket::StartConnect(const net::AddressList& addresses) {
  DCHECK(connection_state_ == CONNECT_IN_PROGRESS);

  socket_.reset(
      new net::TCPClientSocket(addresses, NULL, net::NetLog::Source()));
  int result = socket_->Connect(&connect_callback_);
  if (result != net::ERR_IO_PENDING)
    OnConnectCompleted(result);
}

void PepperTCPSocket::SendConnectACKError() {
  manager_->Send(new PpapiMsg_PPBTCPSocket_ConnectACK(
      routing_id_, plugin_dispatcher_id_, socket_id_, false,
      NetAddressPrivateImpl::kInvalidNetAddress,
      NetAddressPrivateImpl::kInvalidNetAddress));
}

void PepperTCPSocket::SendReadACKError() {
  manager_->Send(new PpapiMsg_PPBTCPSocket_ReadACK(
    routing_id_, plugin_dispatcher_id_, socket_id_, false, std::string()));
}

void PepperTCPSocket::SendWriteACKError() {
  manager_->Send(new PpapiMsg_PPBTCPSocket_WriteACK(
      routing_id_, plugin_dispatcher_id_, socket_id_, false, 0));
}

void PepperTCPSocket::SendSSLHandshakeACK(bool succeeded) {
  manager_->Send(new PpapiMsg_PPBTCPSocket_SSLHandshakeACK(
      routing_id_, plugin_dispatcher_id_, socket_id_, succeeded));
}

void PepperTCPSocket::OnResolveCompleted(int result) {
  DCHECK(connection_state_ == CONNECT_IN_PROGRESS);

  if (result != net::OK) {
    SendConnectACKError();
    connection_state_ = BEFORE_CONNECT;
    return;
  }

  StartConnect(address_list_);
}

void PepperTCPSocket::OnConnectCompleted(int result) {
  DCHECK(connection_state_ == CONNECT_IN_PROGRESS && socket_.get());

  if (result != net::OK) {
    SendConnectACKError();
    connection_state_ = BEFORE_CONNECT;
  } else {
    net::IPEndPoint ip_end_point;
    net::AddressList address_list;
    PP_NetAddress_Private local_addr =
        NetAddressPrivateImpl::kInvalidNetAddress;
    PP_NetAddress_Private remote_addr =
        NetAddressPrivateImpl::kInvalidNetAddress;

    if (socket_->GetLocalAddress(&ip_end_point) != net::OK ||
        !NetAddressPrivateImpl::IPEndPointToNetAddress(ip_end_point,
                                                       &local_addr) ||
        socket_->GetPeerAddress(&address_list) != net::OK ||
        !NetAddressPrivateImpl::AddressListToNetAddress(address_list,
                                                        &remote_addr)) {
      SendConnectACKError();
      connection_state_ = BEFORE_CONNECT;
    } else {
      manager_->Send(new PpapiMsg_PPBTCPSocket_ConnectACK(
          routing_id_, plugin_dispatcher_id_, socket_id_, true,
          local_addr, remote_addr));
      connection_state_ = CONNECTED;
    }
  }
}

void PepperTCPSocket::OnSSLHandshakeCompleted(int result) {
  DCHECK(connection_state_ == SSL_HANDSHAKE_IN_PROGRESS);

  bool succeeded = result == net::OK;
  SendSSLHandshakeACK(succeeded);
  connection_state_ = succeeded ? SSL_CONNECTED : SSL_HANDSHAKE_FAILED;
}

void PepperTCPSocket::OnReadCompleted(int result) {
  DCHECK(read_buffer_.get());

  if (result > 0) {
    manager_->Send(new PpapiMsg_PPBTCPSocket_ReadACK(
        routing_id_, plugin_dispatcher_id_, socket_id_, true,
        std::string(read_buffer_->data(), result)));
  } else if (result == 0) {
    end_of_file_reached_ = true;
    manager_->Send(new PpapiMsg_PPBTCPSocket_ReadACK(
        routing_id_, plugin_dispatcher_id_, socket_id_, true, std::string()));
  } else {
    SendReadACKError();
  }
  read_buffer_ = NULL;
}

void PepperTCPSocket::OnWriteCompleted(int result) {
  DCHECK(write_buffer_.get());

  if (result >= 0) {
    manager_->Send(new PpapiMsg_PPBTCPSocket_WriteACK(
        routing_id_, plugin_dispatcher_id_, socket_id_, true, result));
  } else {
    SendWriteACKError();
  }
  write_buffer_ = NULL;
}

bool PepperTCPSocket::IsConnected() const {
  return connection_state_ == CONNECTED || connection_state_ == SSL_CONNECTED;
}
